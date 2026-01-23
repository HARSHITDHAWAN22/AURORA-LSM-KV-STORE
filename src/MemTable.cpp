#include "MemTable.h"


MemTable::MemTable(int maxEntries)
    : maxEntries(maxEntries) {}


void MemTable::put(const std::string& key, const std::string& value){
    table[key] = value;
}



bool MemTable::get(const std::string& key, std::string& value) const{

    auto it = table.find(key);
    if(it == table.end()){
        return false;
    }
    
    value = it->second;
    return true;
}

void MemTable::remove(const std::string& key){
    table[key] = "__TOMBSTONE__";
}


bool MemTable::isFull() const{
    return static_cast<int>(table.size()) >= maxEntries;
}


void MemTable::clear(){
    table.clear();
}

bool MemTable::isEmpty() const{
    return table.empty();
}


const std::map<std::string, std::string>& MemTable::getData() const{
    return table;
}
