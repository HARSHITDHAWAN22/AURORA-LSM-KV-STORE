#ifndef SSTABLE_H
#define SSTABLE_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <fstream>

#include "BloomFilter.h"
static constexpr uint64_t SSTABLE_MAGIC = 0x4155524F52414B56ULL;


struct SSTableIndexEntry {
    std::string key;
    uint64_t offset;   // line number for text SSTable

    SSTableIndexEntry(const std::string& k, uint64_t o)
        : key(k), offset(o) {}
};

class SSTable {
public:
    SSTable(const std::string& filePath,
            size_t bloomBitSize,
            size_t bloomHashCount);

    bool writeToDisk(const std::map<std::string, std::string>& data);
    bool get(const std::string& key, std::string& value) const;

    const std::string& getFilePath() const;

private:
    std::string filePath;
    BloomFilter bloom;

    // sparse index: key → line number
    mutable std::vector<SSTableIndexEntry> sparseIndex;
    bool isBinarySSTable() const;
bool getBinary(const std::string& key, std::string& value) const;


    void loadBloom();
    void loadSparseIndex();
};

#endif // SSTABLE_H

