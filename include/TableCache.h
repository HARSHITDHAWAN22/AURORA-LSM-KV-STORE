#pragma once
#include <unordered_map>
#include <list>
#include <string>

#include "SSTable.h"

class TableCache {

private:
    size_t capacity;

    std::list<std::string> lruList;

    std::unordered_map<
        std::string,
        std::pair<std::list<std::string>::iterator, SSTable>
    > cache;

public:

    TableCache(size_t cap) : capacity(cap) {}

    bool get(const std::string& file, SSTable& table) {

        auto it = cache.find(file);

        if (it == cache.end())
            return false;

        // Move to front (LRU)
        lruList.erase(it->second.first);
        lruList.push_front(file);

        it->second.first = lruList.begin();

        table = it->second.second;

        return true;
    }

    void put(const std::string& file, const SSTable& table) {

        auto it = cache.find(file);

        if (it != cache.end()) {

            // Update existing
            lruList.erase(it->second.first);
            lruList.push_front(file);

            it->second = {lruList.begin(), table};
            return;
        }

        // Evict if full
        if (cache.size() >= capacity) {

            std::string last = lruList.back();
            lruList.pop_back();
            cache.erase(last);
        }

        // Insert new
        lruList.push_front(file);
        cache.emplace(file,
            std::make_pair(lruList.begin(), table));
    }
};