#include "MemTable.h"
#include "Logger.h"

const std::string MemTable::TOMBSTONE = "__TOMBSTONE__";

MemTable::MemTable(int maxEntries)
    : maxEntries(maxEntries) {
    LOG_INFO("MemTable initialized with maxEntries=" + std::to_string(maxEntries));
}

void MemTable::put(const std::string& key, const std::string& value){
    std::lock_guard<std::mutex> lock(mtx);
    table[key] = value;
}

bool MemTable::get(const std::string& key, std::string& value) const{
    std::lock_guard<std::mutex> lock(mtx);
    auto it = table.find(key);
    if(it == table.end()){
        return false;
    }

    value = it->second;
    return true;
}

void MemTable::remove(const std::string& key){
    std::lock_guard<std::mutex> lock(mtx);
    table[key] = TOMBSTONE;
}

bool MemTable::isFull() const{
    std::lock_guard<std::mutex> lock(mtx);
    return static_cast<int>(table.size()) >= maxEntries;
}

void MemTable::clear(){
    std::lock_guard<std::mutex> lock(mtx);
    table.clear();
    LOG_DEBUG("MemTable cleared");
}

bool MemTable::isEmpty() const{
    std::lock_guard<std::mutex> lock(mtx);
    return table.empty();
}

const std::map<std::string, std::string>& MemTable::getData() const{
    std::lock_guard<std::mutex> lock(mtx);
    return table;
}