#ifndef KV_STORE_H
#define KV_STORE_H

#include<string>
#include<vector>

#include "ConfigManager.h"
#include "MemTable.h"
#include "SSTable.h"
#include "Compaction.h"
#include "BloomFilter.h"
#include "ManifestManager.h"

class KVStore{
public:
    explicit KVStore(const std::string& configPath, const std:: string & strategy);

    // Core operations
    void put(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& value);
    void deleteKey(const std::string& key);

    

    void flush();
void scan(const std::string& start, const std::string& end);


private:
void flushMemTable();
        void runCompactionIfNeeded();
         void loadFromManifest();


    ConfigManager configManager;
    MemTable memTable;

    std::vector<SSTable> sstables;
    Compaction compaction;
     ManifestManager manifest;

    int sstableCounter;

};
/*Also: putting using namespace std; in a header file forces every file that includes it to import the entire std namespace, which can cause name conflicts and unexpected errors in large projects. */
#endif
