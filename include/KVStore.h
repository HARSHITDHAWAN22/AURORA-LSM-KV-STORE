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

class KVStore{
public:
    // MUST MATCH CPP
    explicit KVStore(const std::string& configPath,
                     const std::string& strategy);

    ~KVStore();

    void put(const std::string& key,const std::string& value);
    bool get(const std::string& key,std::string& value);
    void deleteKey(const std::string& key);

    void flush();
    void scan(const std::string& start,const std::string& end);

private:
    void flushMemTable();
    void loadFromManifest();
    void runCompactionIfNeeded();
    void backgroundFlush();

private:
    ConfigManager configManager;

    // POINTER (mutex-safe)
    MemTable* memTable;

    WAL wal;
    std::vector<SSTable> sstables;
    Compaction compaction;
    ManifestManager manifest;

    int sstableCounter;
     void sortSSTablesByAge(); 

    std::thread flushThread;
    std::atomic<bool> running;
    std::mutex flushMutex;
};

#endif