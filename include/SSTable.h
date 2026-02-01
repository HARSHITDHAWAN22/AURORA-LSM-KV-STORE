#ifndef SSTABLE_H
#define SSTABLE_H

#include<string>
#include<map>
#include "BloomFilter.h"


// Immutable on-disk table with an attached Bloom filter

struct SSTableIndexEntry {
    std::string key;
    uint64_t offset;
};





class SSTable{
public:
void loadBloom();

     SSTable(const std::string& filePath,
            size_t bloomBitSize,
            size_t bloomHashCount);

    // Write sorted data to disk
    bool writeToDisk(const std::map<std::string, std::string>& data);

    // Read a key from disk
    bool get(const std::string& key, std::string& value) const;

    const std::string& getFilePath() const;

private:

    std::string filePath;
    BloomFilter bloom;
    std::vector<SSTableIndexEntry> sparseIndex;
    uint64_t dataSectionOffset;   // start of KV records
    mutable std::vector<std::pair<std::string, size_t>> sparseIndex;


};

#endif
