#include "BloomFilter.h"
#include <functional>


BloomFilter::BloomFilter(size_t bitSize, size_t hashCount)
    : bitSize(bitSize),
      hashCount(hashCount),
      bitArray(bitSize, false) {}


void BloomFilter::add(const std::string& key){
    for (size_t i = 0; i < hashCount; ++i) {
        bitArray[hash(key, i) % bitSize] = true;
    }
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
