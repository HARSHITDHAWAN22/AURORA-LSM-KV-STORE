#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <vector>
#include <string>


// Probabilistic data structure for fast membership checks.
// Allows false positives, but never false negatives.
class BloomFilter{

public:

    BloomFilter(size_t bitSize, size_t hashCount);

    // Insert a key into the filter
    void add(const std::string& key);

    // Returns false if key is definitely not present
    // Returns true if key may be present
    bool mightContain(const std::string& key) const;

private:

    size_t hash(const std::string& key, size_t seed) const;

private:

    size_t bitSize;
    size_t hashCount;
    std::vector<bool> bitArray;
};

#endif
