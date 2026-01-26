#include "KVStore.h"
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <chrono>
#include <thread>

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

void KVStore::put(const std::string& key,const std::string& value){
    wal.logPut(key,value);
    memTable->put(key,value);

    if(memTable->isFull()){
        flushMemTable();
    }
}

bool KVStore::get(const std::string& key,std::string& value){
    if(memTable->get(key,value)){
        return true;
    }

    for(auto it = sstables.rbegin(); it != sstables.rend(); ++it){
        if(it->get(key,value)){
            return true;
        }
    }
    return false;
}

void KVStore::deleteKey(const std::string& key){
    wal.logDelete(key);
    memTable->remove(key);

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

    if(sstable.writeToDisk(memTable->getData())){
        sstables.push_back(sstable);
        manifest.addSSTable(filePath);
        manifest.save();

        memTable->clear();
        wal.clear();
        runCompactionIfNeeded();
    }
}

void KVStore::flush(){
    flushMemTable();
}

void KVStore::scan(const std::string& start,const std::string& end){
    std::unordered_set<std::string> seen;

    for(const auto& kv : memTable->getData()){
        if(kv.first >= start && kv.first <= end){
            std::cout << kv.first << " -> " << kv.second << "\n";
            seen.insert(kv.first);
        }
    }

    for(auto it = sstables.rbegin(); it != sstables.rend(); ++it){
        std::ifstream in(it->getFilePath());
        std::string line;

        while(std::getline(in,line)){
            auto pos = line.find(':');
            if(pos == std::string::npos) continue;

            std::string key = line.substr(0,pos);
            std::string value = line.substr(pos+1);

            if(key >= start && key <= end && !seen.count(key)){
                std::cout << key << " -> " << value << "\n";
                seen.insert(key);
            }
        }
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