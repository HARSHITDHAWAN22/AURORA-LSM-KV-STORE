#ifndef KV_STORE_H
#define KV_STORE_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

#include "ConfigManager.h"
#include "MemTable.h"
#include "SSTable.h"
#include "Compaction.h"
#include "ManifestManager.h"
#include "WAL.h"
#include "LRUCache.h"


const int MAX_LEVELS = 4;

struct KVStats{
    uint64_t totalReadSSTables = 0;

    size_t totalPuts = 0;
    size_t totalGets = 0;
    size_t totalFlushes = 0;
    size_t totalCompactions = 0;
    size_t totalBytesWritten = 0;
    size_t totalCompactionBytes = 0;

    uint64_t bloomChecks = 0;
    uint64_t bloomNegatives = 0;
    uint64_t bloomFalsePositives = 0;

     // Cache metrics
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
};

class KVStore : public SSTableStatsHook {
public:

    explicit KVStore(const std::string& configPath,
                     const std::string& strategy);
                     

    ~KVStore();

    void put(const std::string& key,const std::string& value);
    bool get(const std::string& key,std::string& value);
    void deleteKey(const std::string& key);

    void setCompactionStrategy(const std::string& s);

    void flush();
    void scan(const std::string& start,const std::string& end);

    void loadStats();
    void saveStats() const;

    void printStats() const;

    // ---- Bloom filter hook functions ----
    void recordBloomCheck() override {
        stats.bloomChecks++;
    }

    void recordBloomNegative() override {
        stats.bloomNegatives++;
    }

    void recordBloomFalsePositive() override {
        stats.bloomFalsePositives++;
    }

private:

    void flushMemTable();
    void loadFromManifest();
    void runCompactionIfNeeded();
    void backgroundFlush();
    void backgroundCompaction();
    void sortSSTablesByAge();

    std::chrono::steady_clock::time_point lastCompactionTime;

    // ---- statistics ----
    KVStats stats;

    ConfigManager configManager;

    MemTable* memTable;

    WAL wal;

    std::vector<std::vector<SSTable>> levels;

    Compaction compaction;
    ManifestManager manifest;

    int sstableCounter;

    // ---- LRU Cache ----
    LRUCache cache;

    std::thread flushThread;
    std::thread compactionThread;

    std::atomic<bool> running;

    std::mutex flushMutex;
};

#endif