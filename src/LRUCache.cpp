#include "LRUCache.h"

LRUCache::LRUCache(size_t cap){
    capacity = cap;
}

bool LRUCache::get(const std::string& key, std::string& value){

    auto it = cacheMap.find(key);

    if (it == cacheMap.end()) {
        misses++;
        return false;
    }

    hits++;

    cacheList.splice(cacheList.begin(), cacheList, it->second);

    value = it->second->value;

    return true;
}

void LRUCache::put(const std::string& key, const std::string& value){

    auto it = cacheMap.find(key);

    if (it != cacheMap.end()) {

        it->second->value = value;

        cacheList.splice(cacheList.begin(), cacheList, it->second);

        return;
    }

    if(cacheList.size() == capacity){

        auto last = cacheList.back();

        cacheMap.erase(last.key);

        cacheList.pop_back();
    }

    cacheList.push_front({key,value});

    cacheMap[key] = cacheList.begin();
}


void LRUCache::remove(const std::string& key)
{
    auto it = cacheMap.find(key);

    if (it == cacheMap.end())
        return;

    cacheList.erase(it->second);

    cacheMap.erase(it);
}