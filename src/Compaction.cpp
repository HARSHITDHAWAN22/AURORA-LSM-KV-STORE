#include "Compaction.h"

#include <map>
#include <iostream>
#include <vector>
#include <ctime>
#include <algorithm>

#include "SSTableIterator.h"
#include "MergeIterator.h"
#include "MemTable.h"

// static size_t levelCapacity(size_t level) {
//     return 4ULL << level;  // 4, 8, 16, 32...
// }

Compaction::Compaction(Strategy strategy, int maxFilesPerLevel)
    : strategy(strategy),
      maxFilesPerLevel(maxFilesPerLevel) {}

void Compaction::setStrategy(Strategy s) {
    strategy = s;
}

Compaction::Strategy Compaction::getStrategy() const {
    return strategy;
}

size_t Compaction::run(std::vector<std::vector<SSTable>>& levels) {
    if (levels.size() < 2) return 0;   // Need L0 and L1
    size_t level = 0;   // Compact only L0 for now
    const size_t threshold = 4;  // L0 capacity
    if (levels[level].size() < threshold)
        return 0;   // Do NOT compact early
    std::cout << "Compacting L0 -> L1\n";
    std::cout << "Before compaction: L0 size = " << levels[0].size() << ", L1 size = " << levels[1].size() << std::endl;
    // Only compact first 4 files
    size_t filesToCompact = threshold;
    std::vector<SSTableIterator> iters;
    std::vector<Iterator*> inputs;
    iters.reserve(filesToCompact);
    inputs.reserve(filesToCompact);
    for (size_t i = 0; i < filesToCompact; ++i) {
        iters.emplace_back(levels[level][i]);
    }
    for (auto& it : iters) {
        inputs.push_back(&it);
    }
    MergeIterator merge(inputs);
    std::map<std::string, std::string> mergedData;
    size_t bytesWritten = 0;
    while (merge.valid()) {
        if (merge.value() != MemTable::TOMBSTONE) {
            mergedData[merge.key()] = merge.value();
            bytesWritten += merge.key().size() + merge.value().size();
        }
        merge.next();
    }
    std::string outPath =
        "data/L1_" + std::to_string(std::time(nullptr)) + ".dat";
    {
        SSTable writer(outPath, 10000, 3);
        writer.writeToDisk(mergedData);
    }
    SSTable reloaded(outPath, 10000, 3);
    levels[1].push_back(reloaded);
    size_t l0_before = levels[0].size();
    if (levels[0].size() >= filesToCompact) {
        levels[0].erase(
            levels[0].begin(),
            levels[0].begin() + filesToCompact
        );
    }
    size_t l0_after = levels[0].size();
    std::cout << "After erase: L0 size = " << l0_after << ", L1 size = " << levels[1].size() << std::endl;
    if (l0_after >= l0_before) {
        std::cerr << "[ERROR] L0 did not shrink after compaction. Forcing clear of L0!" << std::endl;
        levels[0].clear();
    }
    std::cout << "After compaction: L0 size = " << levels[0].size() << ", L1 size = " << levels[1].size() << std::endl;
    return bytesWritten;
}
