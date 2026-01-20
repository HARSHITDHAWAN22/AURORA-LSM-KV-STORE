#include "KVStore.h"
#include <iostream>




KVStore::KVStore(const std::string& configPath)
    : configManager(configPath),
      memTable(0),
      compaction(
          Compaction::Strategy::LEVEL,
          0
      ),
       manifest("metadata/manifest.txt"),
      sstableCounter(0)
{

    if(!configManager.load()){
        std::cerr<< "Failed to load configuration.\n";
        return;
    }


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

void KVStore::runCompactionIfNeeded(){
    compaction.run(sstables);
}
