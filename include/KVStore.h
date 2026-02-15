#ifndef KV_STORE_H
#define KV_STORE_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

#include "ConfigManager.h"
#include "MemTable.h"
#include "SSTable.h"
#include "Compaction.h"
#include "ManifestManager.h"
#include "WAL.h"

const int MAX_LEVELS = 4;

struct KVStats{
    size_t totalPuts = 0;
    size_t totalGets = 0;
    size_t totalFlushes = 0;
    size_t totalCompactions = 0;
    size_t totalBytesWritten = 0;
    size_t totalCompactionBytes = 0;
};

class KVStore{
public:
    // MUST MATCH CPP
    explicit KVStore(const std::string& configPath,
                     const std::string& strategy);

    ~KVStore();

    void put(const std::string& key,const std::string& value);
    bool get(const std::string& key,std::string& value);
    void deleteKey(const std::string& key);
 void setCompactionStrategy(const std::string& s);
// void KVStore::setCompactionStrategy(const std::string& s);

    void flush();
    void scan(const std::string& start,const std::string& end);
void loadStats();
void saveStats() const;

void printStats() const;

private:
    void flushMemTable();
    void loadFromManifest();
    void runCompactionIfNeeded();
    void backgroundFlush();

    // ---- statistics ----
KVStats stats;





private:
    ConfigManager configManager;

    // POINTER (mutex-safe)
    MemTable* memTable;

    WAL wal;
    std::vector<std::vector<SSTable>>levels;
    static constexpr int MAX_LEVELS = 4;
    


    Compaction compaction;
    ManifestManager manifest;

    int sstableCounter;
     void sortSSTablesByAge(); 

    std::thread flushThread;
    std::atomic<bool> running;
    std::mutex flushMutex;
};

#endif