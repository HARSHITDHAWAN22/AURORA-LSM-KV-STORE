#ifndef SSTABLE_H
#define SSTABLE_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <fstream>

#include "BloomFilter.h"

static constexpr uint64_t SSTABLE_MAGIC = 0x4155524F52414B56ULL;

enum class GetResult {
    NOT_FOUND,
    FOUND,
    DELETED
};

struct SSTableIndexEntry {
    std::string key;
    uint64_t offset;   // offset in binary SSTable
    SSTableIndexEntry(const std::string& k, uint64_t o)
        : key(k), offset(o) {}
};

class SSTable {
public:
    SSTable(const std::string& filePath,
            size_t bloomBitSize,
            size_t bloomHashCount);

    bool writeToDisk(const std::map<std::string, std::string>& data);
    GetResult get(const std::string& key, std::string& value) const;
    const std::string& getFilePath() const;

    void appendKV(const std::string& key,
                  const std::string& value,
                  std::ofstream& out);

    //NEW: Range Metadata Getters
    const std::string& getMinKey() const { return minKey; }
    const std::string& getMaxKey() const { return maxKey; }
    uint64_t getFileSize() const { return fileSize; }

private:
    std::string filePath;

    BloomFilter bloom;
    std::vector<SSTableIndexEntry> sparseIndex;

    //NEW: Range Metadata
    std::string minKey;
    std::string maxKey;
    uint64_t fileSize = 0;

    bool isBinarySSTable() const;
    GetResult getBinary(const std::string& key, std::string& value) const;

    void loadBloom();       // existing
    void loadSparseIndex(); // existing

    // NEW
    void loadFooterMetadata();
};

#endif // SSTABLE_H
