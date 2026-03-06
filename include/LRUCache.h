#pragma once

#include <unordered_map>
#include <list>
#include <string>

class LRUCache{

private:

    struct Node{
        std::string key;
        std::string value;
    };

    size_t capacity;

    std::list<Node> cacheList;

    std::unordered_map<std::string, std::list<Node>::iterator> cacheMap;

public:

    size_t hits = 0;
    size_t misses = 0;

    LRUCache(size_t cap);

    bool get(const std::string& key, std::string& value);

    void put(const std::string& key, const std::string& value);


void remove(const std::string& key);

};