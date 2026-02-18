#include "KVStore.h"
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>

#include "MemTable.h"
#include "MemTableIterator.h"
#include "SSTableIterator.h"
#include "MergeIterator.h"
#include "RangeIterator.h"

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

KVStore::KVStore(const std::string& configPath,
                 const std::string& strategy)
    : configManager(configPath),
      memTable(nullptr),
      wal("metadata/wal.log"),
      compaction(
          strategy == "tiering"
              ? Compaction::Strategy::TIERED
              : Compaction::Strategy::LEVEL,
          0
      ),
      manifest("metadata/manifest.txt"),
      sstableCounter(0),
      running(false)
{
    if (!configManager.load()) {
        std::cerr << "Config load failed\n";
        std::exit(1);
    }

    loadStats();

    memTable = new MemTable(configManager.getMemTableMaxEntries());

    wal.replay(*memTable);

    levels.clear();
    levels.resize(MAX_LEVELS);

    loadFromManifest();

    running = true;
    flushThread = std::thread(&KVStore::backgroundFlush, this);
    compactionThread = std::thread(&KVStore::backgroundCompaction, this);

}

KVStore::~KVStore(){

    running = false;
    saveStats();

    if (flushThread.joinable())
        flushThread.join();

    if (compactionThread.joinable())
        compactionThread.join();

    delete memTable;
}


void KVStore::loadFromManifest(){

    manifest.load();
    auto allLevels = manifest.getLevels();

    for (size_t level = 0; level < allLevels.size(); ++level){

        for (const auto& meta : allLevels[level]){

            SSTable table(
                meta.filePath,
                configManager.getBloomFilterBitSize(),
                configManager.getBloomFilterHashCount()
            );

            levels[level].push_back(table);
            ++sstableCounter;
        }
    }
}

void KVStore::put(const std::string& key,
                  const std::string& value){

    stats.totalPuts++;

    wal.logPut(key, value);
    memTable->put(key, value);

    if (memTable->isFull())
        flushMemTable();
}

bool KVStore::get(const std::string& key,
                  std::string& value){

    stats.totalGets++;

    std::string memVal;

    if(memTable->get(key, memVal)){
        if (memVal == TOMBSTONE)
            return false;

        value = memVal;
        return true;
    }

    for(size_t level = 0; level < levels.size(); ++level){

        auto& tables = levels[level];

        if (level == 0) {

            for(auto it = tables.rbegin();
                 it != tables.rend();
                 ++it) {

                GetResult res = it->get(key, value);

                if (res == GetResult::FOUND) {
                    if (value == TOMBSTONE)
                        return false;
                    return true;
                }

                if (res == GetResult::DELETED)
                    return false;
            }
        }
        else {

            for(auto& table : tables){

                GetResult res = table.get(key, value);

                if(res == GetResult::FOUND){
                    if (value == TOMBSTONE)
                        return false;
                    return true;
                }

                if(res == GetResult::DELETED)
                    return false;
            }
        }
    }

    return false;
}

void KVStore::deleteKey(const std::string& key){

    wal.logDelete(key);
    memTable->put(key, TOMBSTONE);

    if (memTable->isFull())
        flushMemTable();
}


void KVStore::flushMemTable() {

    std::lock_guard<std::mutex> guard(flushMutex);

    if (memTable->isEmpty())
        return;

    std::string filePath =
        configManager.getSSTableDirectory() +
        "/sstable_" + std::to_string(sstableCounter++) + ".dat";

    SSTable sstable(
        filePath,
        configManager.getBloomFilterBitSize(),
        configManager.getBloomFilterHashCount()
    );

    bool flushSuccess = sstable.writeToDisk(memTable->getData());

    if (!flushSuccess) {
        std::cerr << "Flush failed, WAL not cleared!\n";
        return;
    }

    for (const auto& kv : memTable->getData())
        stats.totalBytesWritten += kv.first.size() + kv.second.size();

    SSTable reloaded(
        filePath,
        configManager.getBloomFilterBitSize(),
        configManager.getBloomFilterHashCount()
    );

    levels[0].push_back(reloaded);

    SSTableMeta meta(
        filePath,
        reloaded.getMinKey(),
        reloaded.getMaxKey(),
        reloaded.getFileSize()
    );

    manifest.addSSTable(0, meta);
    manifest.save();

    memTable->clear();
    wal.clear();

    stats.totalFlushes++;

}

void KVStore::flush(){
    if (!memTable->isEmpty())
        flushMemTable();
}

void KVStore::backgroundFlush(){
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        flushMemTable();
    }
}

void KVStore::runCompactionIfNeeded(){

    for (size_t level = 0; level < levels.size() - 1; ++level) {

        if (!manifest.levelOverflow(static_cast<int>(level)))
            continue;

        // Snapshot old files
        std::vector<std::string> oldFiles;

        for (const auto& levelVec : levels) {
            for (const auto& sstable : levelVec) {
                oldFiles.push_back(sstable.getFilePath());
            }
        }

        uint64_t bytes = compaction.run(levels);
stats.totalCompactionBytes += bytes;


        manifest.clear();

        for (size_t lvl = 0; lvl < levels.size(); ++lvl) {

            for (const auto& sstable : levels[lvl]) {

                SSTableMeta meta(
                    sstable.getFilePath(),
                    sstable.getMinKey(),
                    sstable.getMaxKey(),
                    sstable.getFileSize()
                );

                manifest.addSSTable(
                    static_cast<int>(lvl),
                    meta
                );
            }
        }


        


        manifest.save();

        // Delete obsolete files
        for (const auto& file : oldFiles) {

            bool stillExists = false;

            for(const auto& levelVec : levels){
                for(const auto& sstable : levelVec){
                    if(sstable.getFilePath() == file){
                        stillExists = true;
                        break;
                    }
                }
                if (stillExists) break;
            }

            if (!stillExists)
                std::filesystem::remove(file);
        }

        stats.totalCompactions++;

        return; // only one compaction per call
    }
}
void KVStore::backgroundCompaction(){

            while(running){

        std::this_thread::sleep_for(std::chrono::seconds(3));

        runCompactionIfNeeded();
    } }

void KVStore::printStats() const{

    std::cout << "==== AuroraKV Stats ====\n";
    std::cout << "Total PUTs        : " << stats.totalPuts << "\n";
    std::cout << "Total GETs        : " << stats.totalGets << "\n";
    std::cout << "Total Flushes     : " << stats.totalFlushes << "\n";
    std::cout << "Total Compactions : " << stats.totalCompactions << "\n";

    size_t total = 0;
    for (const auto& level : levels)
        total += level.size();

    std::cout << "SSTable Count     : " << total << "\n";
    std::cout << "Bytes Written     : " << stats.totalBytesWritten << "\n";
    std::cout << "Compaction Bytes  : " << stats.totalCompactionBytes << "\n";

    if (stats.totalBytesWritten > 0) {
        double wa =
            (double)(stats.totalBytesWritten + stats.totalCompactionBytes)
            / (double)stats.totalBytesWritten;

        std::cout << "Write Amplification : " << wa << "\n";
    }

    std::cout << "========================\n";
}
