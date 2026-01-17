#include "Compaction.h"
#include <map>
#include <fstream>
#include <iostream>

Compaction::Compaction(Strategy strategy, int maxFilesPerLevel)
    : strategy(strategy),
      maxFilesPerLevel(maxFilesPerLevel) {}

void Compaction::run(std::vector<SSTable>& sstables){
    if(strategy == Strategy::LEVEL){
        runLevelCompaction(sstables);
    } else {
        runTieredCompaction(sstables);
    }
}

// Level-based compaction: triggered when file count exceeds limit
void Compaction::runLevelCompaction(std::vector<SSTable>& sstables){
    if((int)sstables.size() <= maxFilesPerLevel){
        return;
    }

    std::cout << "Running level-based compaction\n";

    std::map<std::string, std::string> mergedData;

    // Merge SSTables, newer entries overwrite older ones
    for(const auto& table : sstables){
        std::ifstream in(table.getFilePath());
        std::string line;

        while(std::getline(in, line)){
            auto pos = line.find(':');
            if (pos == std::string::npos) continue;

            mergedData[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }

    // Remove tombstones
    for(auto it = mergedData.begin(); it != mergedData.end(); ) {
        if (it->second == "__TOMBSTONE__") {
            it = mergedData.erase(it);
        } else {
            ++it;
        }
    }

    SSTable newTable("data/compacted_sstable.dat", 10000, 3);
    newTable.writeToDisk(mergedData);

    sstables.clear();
    sstables.push_back(newTable);
}

// Tiered compaction: merges multiple SSTables in batches
void Compaction::runTieredCompaction(std::vector<SSTable>& sstables){
    if(sstables.size() < 2){
        return;
    }

    std::cout << "Running tiered compaction\n";

    std::map<std::string, std::string> mergedData;


    for(const auto& table : sstables){
        std::ifstream in(table.getFilePath());
        std::string line;


        while(std::getline(in, line)){
            auto pos = line.find(':');
            if (pos == std::string::npos) continue;

            mergedData[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }


    // Remove tombstones
    for(auto it = mergedData.begin(); it != mergedData.end(); ){
        if (it->second == "__TOMBSTONE__") {
            it = mergedData.erase(it);
        } else {
            ++it;
        }
    }

    SSTable newTable("data/tiered_compacted_sstable.dat", 10000, 3);
    newTable.writeToDisk(mergedData);

    sstables.clear();
    sstables.push_back(newTable);
}
