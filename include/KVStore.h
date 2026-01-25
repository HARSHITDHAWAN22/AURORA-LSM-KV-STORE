#ifndef KV_STORE_H
#define KV_STORE_H

#include <string>
#include <vector>

#include "ConfigManager.h"
#include "MemTable.h"
#include "SSTable.h"
#include "Compaction.h"
#include "ManifestManager.h"
#include "WAL.h"

#include<thread>
#include<atomic>

class KVStore{
public:
    explicit KVStore(const std::string& configPath,const std::string& strategy);
    ~KVStore();
    void put(const std::string& key,const std::string& value);
    bool get(const std::string& key,std::string& value);
    void deleteKey(const std::string& key);
    void scan(const std::string& start,const std::string& end);
    void flush();

private:
    void flushMemTable();
    void runCompactionIfNeeded();
    void loadFromManifest();

    ConfigManager configManager;
    MemTable memTable;
    std::vector<SSTable> sstables;
    Compaction compaction;
    ManifestManager manifest;
    WAL wal;                    
    int sstableCounter;

    std::thread flushThread;
    std::atomic<bool> running;

    void backgroundFlush();
};

#endif