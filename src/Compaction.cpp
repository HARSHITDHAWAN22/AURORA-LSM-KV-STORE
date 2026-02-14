#include "Compaction.h"

#include <map>
#include <iostream>
#include <vector>
#include <ctime>

#include "SSTableIterator.h"
#include "MergeIterator.h"
#include "MemTable.h"   // MemTable::TOMBSTONE

#include <filesystem>

static bool isFullCompaction(const std::vector<SSTable>& sstables){
    return sstables.size() > 1;
}

Compaction::Compaction(Strategy strategy, int maxFilesPerLevel)
    : strategy(strategy),
      maxFilesPerLevel(maxFilesPerLevel) {}
void Compaction::setStrategy(Strategy s) {
    strategy = s;
}

Compaction::Strategy Compaction::getStrategy() const {
    return strategy;
}

void Compaction::run(std:: vector<std::vector<SSTable>>& levels){
      // TODO: implement multi-level compaction

    // if(strategy == Strategy::LEVEL){
    //     runLevelCompaction(sstables);
    // } else {
    //     runTieredCompaction(sstables);
    // }
     (void)levels;   // suppress unused warning
}

// ================= LEVEL COMPACTION =================
void Compaction::runLevelCompaction(std::vector<SSTable>& sstables){
    if(static_cast<int>(sstables.size()) <= maxFilesPerLevel){
        return;
    }

    std::cout << "Running level-based compaction\n";

    std::vector<SSTableIterator> iters;
    std::vector<Iterator*> inputs;

    // IMPORTANT: OLDEST → NEWEST
    for(auto it = sstables.begin(); it != sstables.end(); ++it){
        iters.emplace_back(*it);
        inputs.push_back(&iters.back());
    }

    MergeIterator merge(inputs);
    std::map<std::string, std::string> mergedData;

    while(merge.valid()){
        mergedData[merge.key()] = merge.value();
    merge.next();
    }

    std::string outPath =
        "data/compacted_level_" + std::to_string(std::time(nullptr)) + ".dat";

    SSTable newTable(outPath, 10000, 3);
    newTable.writeToDisk(mergedData);
    // optional: print compaction size
auto fileSize = std::filesystem::file_size(outPath);
std::cout << "Compaction output size: "
          << fileSize << " bytes\n";


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

    //  IMPORTANT: OLDEST → NEWEST
    for(auto it = sstables.begin(); it != sstables.end(); ++it){
        iters.emplace_back(*it);
        inputs.push_back(&iters.back());
    }

    MergeIterator merge(inputs);
    std::map<std::string, std::string> mergedData;

  bool full = isFullCompaction(sstables);

while (merge.valid()) {
    if (merge.value() == MemTable::TOMBSTONE) {
        if (!full) {
            // keep tombstone if compaction is partial
            mergedData[merge.key()] = merge.value();
        }
        // else: safe to drop tombstone
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
