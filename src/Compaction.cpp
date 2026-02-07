#include "Compaction.h"

#include <map>
#include <iostream>
#include <vector>
#include <ctime>

#include "SSTableIterator.h"
#include "MergeIterator.h"
#include "MemTable.h"   // MemTable::TOMBSTONE


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

// ================= LEVEL COMPACTION =================
void Compaction::runLevelCompaction(std::vector<SSTable>& sstables){
    if(static_cast<int>(sstables.size()) <= maxFilesPerLevel){
        return;
    }

    std::cout << "Running level-based compaction\n";

    std::vector<SSTableIterator> iters;
    std::vector<Iterator*> inputs;

    // 🔥 IMPORTANT: OLDEST → NEWEST
    for(auto it = sstables.begin(); it != sstables.end(); ++it){
        iters.emplace_back(*it);
        inputs.push_back(&iters.back());
    }

    MergeIterator merge(inputs);
    std::map<std::string, std::string> mergedData;

    while(merge.valid()){
        if(merge.value() == MemTable::TOMBSTONE){
            // tombstone must delete older value
            mergedData.erase(merge.key());
        } else {
            mergedData[merge.key()] = merge.value();
        }
        merge.next();
    }

    std::string outPath =
        "data/compacted_level_" + std::to_string(std::time(nullptr)) + ".dat";

    SSTable newTable(outPath, 10000, 3);
    newTable.writeToDisk(mergedData);

    sstables.clear();
    sstables.push_back(newTable);
}

// ================= TIERED COMPACTION =================
void Compaction::runTieredCompaction(std::vector<SSTable>& sstables){
    if(sstables.size() < 2){
        return;
    }

    std::cout << "Running tiered compaction\n";

    std::vector<SSTableIterator> iters;
    std::vector<Iterator*> inputs;

    // 🔥 IMPORTANT: OLDEST → NEWEST
    for(auto it = sstables.begin(); it != sstables.end(); ++it){
        iters.emplace_back(*it);
        inputs.push_back(&iters.back());
    }

    MergeIterator merge(inputs);
    std::map<std::string, std::string> mergedData;

    while(merge.valid()){
        if(merge.value() == MemTable::TOMBSTONE){
            mergedData.erase(merge.key());
        } else {
            mergedData[merge.key()] = merge.value();
        }
        merge.next();
    }

    std::string outPath =
        "data/compacted_tiered_" + std::to_string(std::time(nullptr)) + ".dat";

    SSTable newTable(outPath, 10000, 3);
    newTable.writeToDisk(mergedData);

    sstables.clear();
    sstables.push_back(newTable);
}
