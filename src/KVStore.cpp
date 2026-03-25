#include "KVStore.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <map>
#include <future>
#include <atomic>
#include <mutex>

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
KVStore::KVStore(const std::string& configPath,
                 const std::string& strategy)

    : configManager(configPath),
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
      tableCache(50),
      cache(10000),
      blockCache(1000),
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

void KVStore::put(const std::string& key,
                  const std::string& value) {

    //std::cout << "[PUT] Start: " << key << "\n";
    stats.totalPuts++;

    //std::cout << "[PUT] WAL write...\n";
    wal.logPut(key, value);
   // std::cout << "[PUT] WAL done\n";

    //std::cout << "[PUT] MemTable insert...\n";
    memTable->put(key, value);
   // std::cout << "[PUT] MemTable done\n";

    //std::cout << "[PUT] Cache put...\n";
    cache.put(key, value);
    //std::cout << "[PUT] Cache done\n";

   // std::cout << "[PUT] Full check...\n";
    if (memTable->isFull()) {
        //std::cout << "[PUT] MemTable full -> flushing...\n";
        flushMemTable();
        //std::cout << "[PUT] Flush done\n";
    }

//    std::cout << "[PUT] End: " << key << "\n";
}

// =======================
// FINAL GET (CORRECT LSM LOGIC)
// =======================
// bool KVStore::get(const std::string& key,
//                   std::string& value) {

//     value.clear();
//     stats.totalGets++;

//     // LRU cache
//     if (cache.get(key, value)) {
//         stats.cacheHits++;
//         return true;
//     }

//     stats.cacheMisses++;

//     // MemTable
//     std::string memVal;
//     if (memTable->get(key, memVal)) {

//         if (memVal == MemTable::TOMBSTONE)
//             return false;

//         value = memVal;
//         cache.put(key, value);
//         return true;
//     }

//     //  LEVEL-WISE SEARCH (FIXED)
//     for (size_t level = 0; level < levels.size(); level++) {

//         std::atomic<bool> found(false);
//         std::string levelResult;
//         std::mutex levelMutex;

//         std::vector<std::future<void>> futures;

//         for (auto& tableRef : levels[level]) {

//             futures.push_back(std::async(std::launch::async,
//                 [&, this, key, tableRef]() {

//                 if (found.load()) return;

//                 if (key < tableRef.getMinKey() || key > tableRef.getMaxKey())
//                     return;

//                 // block cache
//                 std::string blockKey =
//                     tableRef.getFilePath() + "_" + key;

//                 std::string cachedValue;
//                 if (blockCache.get(blockKey, cachedValue)) {
//                     if (!found.exchange(true)) {
//                         std::lock_guard<std::mutex> lock(levelMutex);
//                         levelResult = cachedValue;
//                     }
//                     return;
//                 }

//                 // bloom
//                 if (!tableRef.mightContain(key))
//                     return;

//                 std::string val;
//                 GetResult res;

//                 SSTable cached = tableRef;

//                 if (tableCache.get(tableRef.getFilePath(), cached)) {
//                     res = cached.get(key, val);
//                 } else {
//                     tableCache.put(tableRef.getFilePath(), tableRef);
//                     res = tableRef.get(key, val);
//                 }

//                 //  DELETE DOMINATES
//                 if (res == GetResult::DELETED) {
//                     found = true;
//                     std::lock_guard<std::mutex> lock(levelMutex);
//                     levelResult.clear();
//                     return;
//                 }

//                 if (res == GetResult::FOUND) {

//                     blockCache.put(blockKey, val);

//                     if (!found.exchange(true)) {
//                         std::lock_guard<std::mutex> lock(levelMutex);
//                         levelResult = val;
//                     }
//                 }
//             }));
//         }

//         for (auto& f : futures) f.get();

//         // stop at first level
//         if (found) {
//             if (!levelResult.empty()) {
//                 value = levelResult;
//                 cache.put(key, value);
//                 return true;
//             }
//             return false;
//         }
//     }

//     return false;
// }

bool KVStore::get(const std::string& key, std::string& value) {
    value.clear();
    stats.totalGets++;

    // LRU cache
    if (cache.get(key, value)) {
        stats.cacheHits++;
        return true;
    }

    stats.cacheMisses++;

    // MemTable
    std::string memVal;
    if (memTable->get(key, memVal)) {
        if (memVal == MemTable::TOMBSTONE)
            return false;

        value = memVal;
        cache.put(key, value);
        return true;
    }

    // LEVEL-WISE SEARCH (sequential, stable)
    for (size_t level = 0; level < levels.size(); level++) {

        for (auto& tableRef : levels[level]) {

            if (key < tableRef.getMinKey() || key > tableRef.getMaxKey())
                continue;

            // block cache
            std::string blockKey = tableRef.getFilePath() + "_" + key;
            std::string cachedValue;

            if (blockCache.get(blockKey, cachedValue)) {
                value = cachedValue;
                cache.put(key, value);
                return true;
            }

            // bloom
            if (!tableRef.mightContain(key))
                continue;

            std::string val;
            GetResult res;

            SSTable cached = tableRef;

            if (tableCache.get(tableRef.getFilePath(), cached)) {
                res = cached.get(key, val);
            } else {
                tableCache.put(tableRef.getFilePath(), tableRef);
                res = tableRef.get(key, val);
            }

            // DELETE dominates
            if (res == GetResult::DELETED) {
                return false;
            }

            if (res == GetResult::FOUND) {
                blockCache.put(blockKey, val);
                value = val;
                cache.put(key, value);
                return true;
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