#include "Compaction.h"
#include <map>
#include <iostream>


// Constructor
Compaction::Compaction(Strategy strategy, int maxFilesPerLevel)
    : strategy(strategy),
      maxFilesPerLevel(maxFilesPerLevel) {}

// Run compaction based on selected strategy
void Compaction::run(std::vector<SSTable>& sstables) {
    if (strategy == Strategy::LEVEL) {
        runLevelCompaction(sstables);
    } else {
        runTieredCompaction(sstables);
    }
}


// Level-based compaction: trigger when file count exceeds limit
void Compaction::runLevelCompaction(std::vector<SSTable>& sstables) {
    if((int)sstables.size() <= maxFilesPerLevel){
        return;
    }

    std::cout << "Running level-based compaction\n";
    // Actual merge logic will be added next
}


// Tiered compaction: merge files in size-based batches
void Compaction::runTieredCompaction(std::vector<SSTable>& sstables) {
    if (sstables.size() < 2) {
        return;
    }

    
    std::cout << "Running tiered compaction\n";
    // Actual merge logic will be added next
}
