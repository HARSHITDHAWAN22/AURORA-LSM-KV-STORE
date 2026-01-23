#ifndef SSTABLE_H
#define SSTABLE_H

#include<string>
#include<map>
#include "BloomFilter.h"

// Immutable on-disk table with an attached Bloom filter

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
};

#endif
