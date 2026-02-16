#include "BloomFilter.h"
#include <functional>
#include <fstream>
#include<bits/stdc++.h>
#include <cstdint>



BloomFilter::BloomFilter(size_t bitSize, size_t hashCount)
    : bitSize(bitSize),
      hashCount(hashCount),
      bitArray(bitSize,0) {}


void BloomFilter::add(const std::string& key){
    for (size_t i = 0; i < hashCount; ++i) {
        bitArray[hash(key, i) % bitSize] = 1;
    }
}

void BloomFilter::serialize(std::ofstream& out) const{
    uint64_t size = bitArray.size();
    out.write(reinterpret_cast<const char*>(&size), sizeof(size));
    out.write(reinterpret_cast<const char*>(bitArray.data()), size);
}

bool BloomFilter::mightContain(const std::string& key) const{
    for(size_t i = 0; i < hashCount; ++i){
        if(!bitArray[hash(key, i) % bitSize]){
            return false;
        }
    }
    return true;
}

size_t BloomFilter::hash(const std::string& key, size_t seed) const{
    return std::hash<std::string>{}(key + std::to_string(seed));
}
