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

    //SAFE: heap allocation (no mutex copy)
    memTable = new MemTable(configManager.getMemTableMaxEntries());

    // WAL recovery
    wal.replay(*memTable);

    loadFromManifest();
    sortSSTablesByAge();

    running = true;
    flushThread = std::thread(&KVStore::backgroundFlush, this);
}

KVStore::~KVStore(){
    running = false;
    if(flushThread.joinable())
        flushThread.join();

    delete memTable;
}

void KVStore::loadFromManifest(){
    manifest.load();

    for(const auto& path : manifest.getSSTables()){
        sstables.emplace_back(
            path,
            configManager.getBloomFilterBitSize(),
            configManager.getBloomFilterHashCount()
        );
        ++sstableCounter;
    }
}
void KVStore::sortSSTablesByAge() {
    std::sort(
        sstables.begin(),
        sstables.end(),
        [](const SSTable& a, const SSTable& b) {
            return a.getFilePath() < b.getFilePath();
        }
    );
}

void KVStore::put(const std::string& key,const std::string& value){
    wal.logPut(key,value);
    memTable->put(key,value);

    if(memTable->isFull()){
        flushMemTable();
    }
}

bool KVStore::get(const std::string& key,std::string& value){
    if(memTable->get(key,value)){
         if(value == TOMBSTONE) return false;
        return true;
    }

    for(auto it = sstables.rbegin(); it != sstables.rend(); ++it){
        if(it->get(key,value)){
              if(value == TOMBSTONE) return false;
            return true;
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

    if (sstable.writeToDisk(memTable->getData())) {

    // 🔥 IMPORTANT FIX
    SSTable reloaded(
        filePath,
        configManager.getBloomFilterBitSize(),
        configManager.getBloomFilterHashCount()
    );

    sstables.push_back(reloaded);

    manifest.addSSTable(filePath);
    manifest.save();

    memTable->clear();
    wal.clear();
    runCompactionIfNeeded();
}

}

void KVStore::flush(){
    flushMemTable();
    sortSSTablesByAge();
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
    for(auto it = sstables.rbegin(); it != sstables.rend(); ++it){
        sstIters.emplace_back(*it);
        inputs.push_back(&sstIters.back());
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
    compaction.run(sstables);
}