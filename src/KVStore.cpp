#include "KVStore.h"
#include<iostream>

KVStore::KVStore(const std::string& configPath)
    : configManager(configPath),
      memTable(0)   // temporary init

{

    if(!configManager.load()){
        std::cerr<< "Failed to load configuration." << std::endl;
        return;
    }


    // Reinitialize MemTable with config value
    memTable = MemTable(configManager.getMemTableMaxEntries());

}

void KVStore::put(const std::string& key, const std::string& value){
    memTable.put(key, value);

    if(memTable.isFull()){
        std::cout << "MemTable full. Flush to disk will be implemented next.\n";
        memTable.clear();

    }
}

bool KVStore::get(const std::string& key, std::string& value){

    return memTable.get(key, value);
}

void KVStore::deleteKey(const std::string& key){
    
    memTable.remove(key);
}
