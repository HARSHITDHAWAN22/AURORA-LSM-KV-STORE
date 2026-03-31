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
    uint64_t offset;
    SSTableIndexEntry(const std::string& k, uint64_t o)
        : key(k), offset(o) {}
};

class SSTableStatsHook {
public:
    virtual void recordBloomCheck() = 0;
    virtual void recordBloomNegative() = 0;
    virtual void recordBloomFalsePositive() = 0;
    virtual ~SSTableStatsHook() = default;
};

class SSTable {
public:
    // =======================
    // CONSTRUCTOR
    // =======================
    SSTable(const std::string& filePath,
            size_t bloomBitSize,
            size_t bloomHashCount);

    // =======================
    // COPY CONSTRUCTOR (FIXED ORDER)
    // =======================
    SSTable(const SSTable& other)
        : statsHook(other.statsHook),
          filePath(other.filePath),
          bloom(other.bloom),
          sparseIndex(other.sparseIndex),
          minKey(other.minKey),
          maxKey(other.maxKey),
          fileSize(other.fileSize) {}

    // =======================
    // ASSIGNMENT OPERATOR
    // =======================
    SSTable& operator=(const SSTable& other) {
        if (this != &other) {
            statsHook = other.statsHook;
            filePath = other.filePath;
            bloom = other.bloom;
            sparseIndex = other.sparseIndex;
            minKey = other.minKey;
            maxKey = other.maxKey;
            fileSize = other.fileSize;
        }
        return *this;
    }

    // =======================
    // CORE APIs
    // =======================
    bool writeToDisk(const std::map<std::string, std::string>& data);
    GetResult get(const std::string& key, std::string& value) const;
    const std::string& getFilePath() const;

    void appendKV(const std::string& key,
                  const std::string& value,
                  std::ofstream& out);

    // =======================
    // RANGE METADATA
    // =======================
    const std::string& getMinKey() const { return minKey; }
    const std::string& getMaxKey() const { return maxKey; }
    uint64_t getFileSize() const { return fileSize; }

    // =======================
    // BLOOM FILTER ENTRY
    // =======================
    bool mightContain(const std::string& key) const;

    void setStatsHook(SSTableStatsHook* hook) {
        statsHook = hook;
    }

private:
    // =======================
    // MEMBER ORDER (IMPORTANT)
    // =======================
    SSTableStatsHook* statsHook = nullptr;   // 1 FIRST

    std::string filePath;                // 2
    BloomFilter bloom;                     // 3
    std::vector<SSTableIndexEntry> sparseIndex; // 4

    std::string minKey;                     // 5
    std::string maxKey;                     // 6
    uint64_t fileSize = 0;                  // 7 LAST

    // =======================
    // INTERNALS
    // =======================
    void loadFooterMetadata();
    bool isBinarySSTable() const;
    GetResult getBinary(const std::string& key, std::string& value) const;

    void loadBloom();
    void loadSparseIndex();
};

#endif // SSTABLE_H