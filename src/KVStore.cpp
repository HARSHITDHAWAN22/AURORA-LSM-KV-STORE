#include "KVStore.h"
#include <iostream>
#include <fstream>
#include <unordered_set>
#include<filesystem>

KVStore::KVStore(const std::string& configPath)
    : configManager(configPath),
      memTable(0),
      compaction(Compaction::Strategy::LEVEL, 0),
      manifest("metadata/manifest.txt"),
      sstableCounter(0)
{
    if(!configManager.load()){
        std::cerr << "Failed to load configuration.\n";
        return;
    }

    std::filesystem::create_directories(
    configManager.getSSTableDirectory()
);
std::filesystem::create_directories("metadata");


    memTable = MemTable(configManager.getMemTableMaxEntries());

    compaction = Compaction(
        Compaction::Strategy::LEVEL,
        configManager.getMaxFilesPerLevel()
    );

    loadFromManifest();
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

void KVStore::put(const std::string& key, const std::string& value){
    memTable.put(key, value);

    if(memTable.isFull()){
        flushMemTable();
    }
}

bool KVStore::get(const std::string& key, std::string& value){
    if(memTable.get(key, value)){
        return true;
    }

    for(auto it = sstables.rbegin(); it != sstables.rend(); ++it){
        if(it->get(key, value)){
            return true;
        }
    }

    return false;
}

void KVStore::deleteKey(const std::string& key){
    memTable.remove(key);

    if(memTable.isFull()){
        flushMemTable();
    }
}

void KVStore::flushMemTable(){
    std::string filePath =
        configManager.getSSTableDirectory() +
        "/sstable_" + std::to_string(sstableCounter++) + ".dat";

    SSTable sstable(
        filePath,
        configManager.getBloomFilterBitSize(),
        configManager.getBloomFilterHashCount()
    );

    if(sstable.writeToDisk(memTable.getData())){
        sstables.push_back(sstable);
        manifest.addSSTable(filePath);
        manifest.save();
        memTable.clear();
        runCompactionIfNeeded();
    }
}

void KVStore::flush(){
    if(!memTable.isEmpty()){
        flushMemTable();
    }
}

void KVStore::scan(const std::string& start, const std::string& end){
    std::unordered_set<std::string> seen;

    // 1) MemTable (newest data)
    for(const auto& kv : memTable.getData()){
        if(kv.first >= start && kv.first <= end){
            std::cout << kv.first << " -> " << kv.second << "\n";
            seen.insert(kv.first);
        }
    }

    // 2) SSTables (newest to oldest)
    for(auto it = sstables.rbegin(); it != sstables.rend(); ++it){
        std::ifstream in(it->getFilePath());
        std::string line;

        while(std::getline(in, line)){
            auto pos = line.find('\t');
            if(pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            if(key >= start && key <= end && !seen.count(key)){
                std::cout << key << " -> " << value << "\n";
                seen.insert(key);
            }
        }
    }
}

void KVStore::runCompactionIfNeeded(){
    compaction.run(sstables);
}
