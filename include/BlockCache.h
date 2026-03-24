#ifndef BLOCK_CACHE_H
#define BLOCK_CACHE_H

#include <unordered_map>
#include <list>
#include <string>

class BlockCache {
private:
    size_t capacity;

    std::list<std::pair<std::string, std::string>> lru;

    std::unordered_map<std::string,
        std::list<std::pair<std::string, std::string>>::iterator> map;

public:
    BlockCache(size_t cap) : capacity(cap) {}

    bool get(const std::string& key, std::string& value) {
        auto it = map.find(key);
        if (it == map.end()) return false;

        lru.splice(lru.begin(), lru, it->second);
        value = it->second->second;
        return true;
    }

    void put(const std::string& key, const std::string& value) {
        auto it = map.find(key);

        if (it != map.end()) {
            lru.erase(it->second);
            map.erase(it);
        }

        lru.push_front({key, value});
        map[key] = lru.begin();

        if (map.size() > capacity) {
            auto last = lru.back();
            map.erase(last.first);
            lru.pop_back();
        }
    }
};

#endif