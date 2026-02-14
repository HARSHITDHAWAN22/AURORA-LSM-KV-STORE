#include "KVStore.h"
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <chrono>
#include <thread>

#include "MemTable.h"
#include<math.h>
#include<algorithm>
#include "MemTableIterator.h"
#include "SSTableIterator.h"
#include "MergeIterator.h"
#include "RangeIterator.h"

#include <filesystem>



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
    if(!configManager.load()){
        std::cerr << "Config load failed\n";
        std::exit(1);
    }
loadStats();

    //SAFE: heap allocation (no mutex copy)
    memTable = new MemTable(configManager.getMemTableMaxEntries());

    // WAL recovery
wal.replay(*memTable);

// Initialize LSM levels FIRST
levels.clear();
levels.resize(MAX_LEVELS);

// Load existing SSTables
loadFromManifest();


    //sortSSTablesByAge();

    running = true;
    flushThread = std::thread(&KVStore::backgroundFlush, this);
}

KVStore::~KVStore(){
    running = false;
    saveStats();

    if(flushThread.joinable())
        flushThread.join();

    delete memTable;
} 
void KVStore::setCompactionStrategy(const std::string& s) {
    if (s == "tiering") {
        compaction.setStrategy(Compaction::Strategy::TIERED);
    } else {
        compaction.setStrategy(Compaction::Strategy::LEVEL);
    }
}

void KVStore::loadFromManifest(){
    manifest.load();
    auto allSSTables = manifest.getAllSSTables();
    for(size_t level = 0; level < allSSTables.size(); ++level){
        for(const auto& path : allSSTables[level]){
            SSTable table(
                path,
                configManager.getBloomFilterBitSize(),
                configManager.getBloomFilterHashCount()
            );
            levels[level].push_back(table);
            ++sstableCounter;
        }
    }
}

// void KVStore::sortSSTablesByAge() {
//     std::sort(
//         sstables.begin(),
//         sstables.end(),
//         [](const SSTable& a, const SSTable& b) {
//             return std::stoi(a.getFilePath().substr(a.getFilePath().find_last_of('_') + 1)) <
//        std::stoi(b.getFilePath().substr(b.getFilePath().find_last_of('_') + 1));

//         }
//     );
// }

void KVStore::put(const std::string& key,const std::string& value){
   stats.totalPuts++;


    wal.logPut(key,value);
    memTable->put(key,value);

    if(memTable->isFull()){
        flushMemTable();
    }
}

bool KVStore::get(const std::string& key,std::string& value){
    {stats.totalGets++;

    std::string memVal;
    if (memTable->get(key, memVal)) {
        if (memVal == TOMBSTONE)
            return false;

        value = memVal;
        return true;
    }
}


    for(size_t level = 0; level < levels.size(); ++level){

    auto& tables = levels[level];

    // L0 → newest first
    if(level == 0){

        for(auto it = tables.rbegin(); it != tables.rend(); ++it){
            GetResult res = it->get(key, value);

            if (res == GetResult::FOUND) return true;
            if (res == GetResult::DELETED) return false;
        }
    }

    else{
        // Higher levels: oldest-first (non-overlapping)

        for(auto& table : tables){
            GetResult res = table.get(key, value);

            if (res == GetResult::FOUND) return true;
            if (res == GetResult::DELETED) return false;
        }
    }
}

    return false;
}

void KVStore::deleteKey(const std::string& key){
    wal.logDelete(key);
    memTable->put(key, TOMBSTONE);


    if(memTable->isFull()){
        flushMemTable();
    }
}

void KVStore::flushMemTable(){
    std::lock_guard<std::mutex> guard(flushMutex);
    if(memTable->isEmpty())
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
    if (flushSuccess) {
        for (const auto& kv : memTable->getData()) {
            stats.totalBytesWritten += kv.first.size() + kv.second.size();
        }
        SSTable reloaded(
            filePath,
            configManager.getBloomFilterBitSize(),
            configManager.getBloomFilterHashCount()
        );
        levels[0].push_back(reloaded);
        manifest.addSSTable(0, filePath);
        manifest.save();
        memTable->clear();
        wal.clear();
        stats.totalFlushes++;
        std::cout << "Flush: L0 size = " << levels[0].size() << std::endl;
        runCompactionIfNeeded();
    } else {
        std::cerr << "Flush failed, WAL not cleared!" << std::endl;
    }
}

void KVStore::flush(){
     if(!memTable->isEmpty())
    flushMemTable();
  //  sortSSTablesByAge();
}

void KVStore::scan(const std::string& start,const std::string& end){
    // create iterators
    std::vector<Iterator*> inputs;

    // MemTable iterator (highest priority)
    MemTableIterator memIter(
        memTable->getData().cbegin(),
        memTable->getData().cend()
    );
    inputs.push_back(&memIter);

    // SSTable iterators (newest first = higher priority)
    std::vector<SSTableIterator> sstIters;
    for(size_t level = 0; level < levels.size(); ++level){
        auto& tables = levels[level];
        for(auto it = tables.rbegin(); it != tables.rend(); ++it){
            sstIters.emplace_back(*it);
            inputs.push_back(&sstIters.back());
        }
    }

    // merge iterator
    MergeIterator merge(inputs);

    // range filter 
    RangeIterator range(&merge, start, end);

    // output
    while(range.valid()){
        if(range.value() != TOMBSTONE){
            std::cout << range.key()
                      << " -> "
                      << range.value()
                      << "\n";
        }
        range.next(); 
    }
}


void KVStore::backgroundFlush(){
    while(running){
        std::this_thread::sleep_for(std::chrono::seconds(2));
        flushMemTable();
    }
}

void KVStore::runCompactionIfNeeded(){
    size_t l0Threshold = 4; // match Compaction.cpp
    std::cout << "runCompactionIfNeeded: L0 size = " << levels[0].size() << std::endl;
    if (levels[0].size() >= l0Threshold) {
        size_t bytesWritten = compaction.run(levels);
        // After compaction, update manifest for all levels (only live SSTables)
        manifest.clear();
        for(size_t level = 0; level < levels.size(); ++level){
            for(const auto& sstable : levels[level]){
                manifest.addSSTable((int)level, sstable.getFilePath());
            }
        }
        manifest.save();
        stats.totalCompactions++;
        stats.totalCompactionBytes += bytesWritten;
    }
}


void KVStore::printStats() const {
    std::cout << "==== AuroraKV Stats ====\n";
    std::cout << "Total PUTs        : " << stats.totalPuts << "\n";
    std::cout << "Total GETs        : " << stats.totalGets << "\n";
    std::cout << "Total Flushes     : " << stats.totalFlushes << "\n";
    std::cout << "Total Compactions : " << stats.totalCompactions << "\n";
    size_t total = 0;
    for(const auto& level : levels)
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

// NOTE: Manifest recovery only loads L0. If compaction moves files to L1+, those are not recovered. Consider extending manifest to track all levels for full recovery.
