#include "KVStore.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <map>

#include "MemTable.h"
#include "Logger.h"
#include "TableCache.h"

static std::chrono::steady_clock::time_point benchmarkStart;

// =======================
void KVStore::loadStats() {
    std::ifstream in("metadata/stats.dat", std::ios::binary);
    if (!in.is_open()) return;
    in.read(reinterpret_cast<char*>(&stats), sizeof(stats));
}

void KVStore::saveStats() const {
    std::ofstream out("metadata/stats.dat",
                      std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(&stats),
              sizeof(stats));
}

// =======================
// FIXED CONSTRUCTOR
// =======================
KVStore::KVStore(const std::string& configPath,
                 const std::string& strategy)

    : tableCache(50),
      configManager(configPath),
      memTable(nullptr),
      wal("metadata/wal.log"),
      levels(),
      compaction(
          strategy == "tiering"
              ? Compaction::Strategy::TIERED
              : Compaction::Strategy::LEVEL,
          0),
      manifest("metadata/manifest.txt"),
      sstableCounter(0),
      cache(10000),
      running(false)
{
    if (!configManager.load()) {
        throw std::runtime_error("Config load failed");
    }

    loadStats();
    benchmarkStart = std::chrono::steady_clock::now();

    memTable = new MemTable(configManager.getMemTableMaxEntries());

    wal.replay(*memTable);

    levels.resize(MAX_LEVELS);
    loadFromManifest();

    running = true;

    flushThread = std::thread(&KVStore::backgroundFlush, this);
}

// =======================
KVStore::~KVStore() {

    running = false;

    saveStats();

    if (flushThread.joinable()) flushThread.join();

    delete memTable;
}

// =======================
void KVStore::loadFromManifest() {

    manifest.load();
    auto allLevels = manifest.getLevels();

    for (size_t level = 0; level < allLevels.size(); ++level) {
        for (const auto& meta : allLevels[level]) {

            SSTable table(
                meta.filePath,
                configManager.getBloomFilterBitSize(),
                configManager.getBloomFilterHashCount()
            );

            table.setStatsHook(this);

            levels[level].push_back(table);
            ++sstableCounter;
        }
    }
}

// =======================
void KVStore::put(const std::string& key,
                  const std::string& value) {

    stats.totalPuts++;

    wal.logPut(key, value);
    memTable->put(key, value);

    cache.put(key, value);

    if (memTable->isFull())
        flushMemTable();
}

// =======================
//  FINAL FIXED GET
// =======================
bool KVStore::get(const std::string& key,
                  std::string& value) {

    value.clear();
    stats.totalGets++;

    if (cache.get(key, value)) {
        stats.cacheHits++;
        return true;
    }

    stats.cacheMisses++;

    std::string memVal;
    if (memTable->get(key, memVal)) {

        if (memVal == MemTable::TOMBSTONE)
            return false;

        value = memVal;
        cache.put(key, value);
        return true;
    }

    for (size_t level = 0; level < levels.size(); level++) {

        auto& tables = levels[level];

        if (level == 0) {

            for (auto it = tables.rbegin(); it != tables.rend(); ++it) {

                if (key < it->getMinKey() || key > it->getMaxKey())
                    continue;

                SSTable& table = *it;

                // 🔥 ONLY THIS (NO manual stats)
                if (!table.mightContain(key))
                    continue;

                SSTable cached = table;

                if (tableCache.get(it->getFilePath(), cached)) {

                    GetResult res = cached.get(key, value);

                    if (res == GetResult::FOUND) {
                        cache.put(key, value);
                        return true;
                    }

                    if (res == GetResult::DELETED) {
                        return false;
                    }

                } else {

                    tableCache.put(it->getFilePath(), table);

                    GetResult res = table.get(key, value);

                    if (res == GetResult::FOUND) {
                        cache.put(key, value);
                        return true;
                    }

                    if (res == GetResult::DELETED) {
                        return false;
                    }
                }
            }
        }
        else {

            for (auto& t : tables) {

                if (key < t.getMinKey() || key > t.getMaxKey())
                    continue;

                SSTable& table = t;

                if (!table.mightContain(key))
                    continue;

                SSTable cached = table;

                if (tableCache.get(t.getFilePath(), cached)) {

                    GetResult res = cached.get(key, value);

                    if (res == GetResult::FOUND) {
                        cache.put(key, value);
                        return true;
                    }

                    if (res == GetResult::DELETED) {
                        return false;
                    }

                } else {

                    tableCache.put(t.getFilePath(), table);

                    GetResult res = table.get(key, value);

                    if (res == GetResult::FOUND) {
                        cache.put(key, value);
                        return true;
                    }

                    if (res == GetResult::DELETED) {
                        return false;
                    }
                }
            }
        }
    }

    return false;
}

// =======================
void KVStore::deleteKey(const std::string& key) {

    wal.logDelete(key);
    memTable->put(key, MemTable::TOMBSTONE);

    cache.remove(key);

    if (memTable->isFull())
        flushMemTable();
}

// =======================
void KVStore::flushMemTable() {

    std::lock_guard<std::mutex> guard(flushMutex);

    if (memTable->isEmpty()) return;

    std::string filePath =
        configManager.getSSTableDirectory() +
        "/sstable_" + std::to_string(sstableCounter++) + ".dat";

    SSTable sstable(
        filePath,
        configManager.getBloomFilterBitSize(),
        configManager.getBloomFilterHashCount()
    );

    sstable.writeToDisk(memTable->getData());

    SSTable reloaded(
        filePath,
        configManager.getBloomFilterBitSize(),
        configManager.getBloomFilterHashCount()
    );

    reloaded.setStatsHook(this);

    levels[0].push_back(reloaded);

    memTable->clear();
    wal.clear();

    stats.totalFlushes++;
}

// =======================
void KVStore::flush() {
    if (!memTable->isEmpty())
        flushMemTable();
}

// =======================
void KVStore::backgroundFlush() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        flushMemTable();
    }
}

// =======================
void KVStore::scan(const std::string& start,
                   const std::string& end) {

    std::map<std::string, std::string> result;

    for (const auto& kv : memTable->getData()) {
        if (kv.first >= start && kv.first <= end)
            result[kv.first] = kv.second;
    }

    for (char c = start[0]; c <= end[0]; c++) {

        std::string key(1, c);
        std::string value;

        for (auto& level : levels) {
            for (auto& table : level) {

                if (key < table.getMinKey() || key > table.getMaxKey())
                    continue;

                if (!table.mightContain(key)) continue;

                GetResult res = table.get(key, value);

                if (res == GetResult::FOUND)
                    result[key] = value;

                if (res == GetResult::DELETED)
                    result.erase(key);
            }
        }
    }

    for (const auto& kv : result) {

        if (kv.second == MemTable::TOMBSTONE)
            continue;

        std::cout << kv.first << " -> " << kv.second << "\n";
    }
}

// =======================
void KVStore::printStats() const {

    std::cout << "==== AuroraKV Stats ====\n";

    auto now = std::chrono::steady_clock::now();

    double seconds =
        std::chrono::duration<double>(now - benchmarkStart).count();

    if (seconds < 1) seconds = 1;

    std::cout << "PUT Throughput : "
              << stats.totalPuts / seconds << "\n";

    std::cout << "GET Throughput : "
              << stats.totalGets / seconds << "\n";

    std::cout << "Cache Hit Rate : "
              << (double)stats.cacheHits /
                 (stats.cacheHits + stats.cacheMisses + 1)
              << "\n";

    std::cout << "Bloom Checks : " << stats.bloomChecks << "\n";
    std::cout << "Bloom Negatives : " << stats.bloomNegatives << "\n";
    std::cout << "Bloom False Positives : " << stats.bloomFalsePositives << "\n";
}