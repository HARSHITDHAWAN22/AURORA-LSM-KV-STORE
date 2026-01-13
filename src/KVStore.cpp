#ifndef KV_STORE_H
#define KV_STORE_H

#include <string>
#include "ConfigManager.h"

class KVStore{
public:
    explicit KVStore(const std::string& configPath);

    // Core operations
    void put(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& value);
    void deleteKey(const std::string& key);

    
private:
    ConfigManager configManager;

    // Future components (placeholders)
    // MemTable* memTable;
    // Compaction* compaction;
};

#endif
