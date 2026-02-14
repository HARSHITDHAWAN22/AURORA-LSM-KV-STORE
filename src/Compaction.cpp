#include "Compaction.h"

#include <map>
#include <iostream>
#include <vector>
#include <ctime>
#include <algorithm>
#include <cstdio> // for std::remove

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
    // Take a snapshot of the file paths to compact
    size_t filesToCompact = threshold;
    std::vector<std::string> filesToCompactPaths;
    for (size_t i = 0; i < filesToCompact && i < levels[0].size(); ++i) {
        filesToCompactPaths.push_back(levels[0][i].getFilePath());
    }
    // Build SSTable objects for compaction from file paths
    std::vector<SSTable> sstablesToCompact;
    for (const auto& filePath : filesToCompactPaths) {
        std::ifstream testFile(filePath, std::ios::binary);
        if (!testFile.is_open()) {
            std::cerr << "[ERROR] SSTable file missing: " << filePath << std::endl;
            continue;
        }
        sstablesToCompact.emplace_back(filePath, 10000, 3);
    }
    std::vector<SSTableIterator> iters;
    std::vector<Iterator*> inputs;
    for (auto& sstable : sstablesToCompact) {
        iters.emplace_back(sstable);
    }
    for (auto& it : iters) {
        inputs.push_back(&it);
    }
    if (inputs.empty()) {
        std::cerr << "[ERROR] No valid SSTables to compact!" << std::endl;
        return 0;
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
    // Now erase the compacted SSTables from L0 and delete their files
    for (const auto& filePath : filesToCompactPaths) {
        auto it = std::find_if(levels[0].begin(), levels[0].end(), [&](const SSTable& s) { return s.getFilePath() == filePath; });
        if (it != levels[0].end()) {
            std::cout << "Deleting compacted SSTable file: " << filePath << std::endl;
            std::remove(filePath.c_str());
            levels[0].erase(it);
        }
    }
    size_t l0_after = levels[0].size();
    std::cout << "After erase: L0 size = " << l0_after << ", L1 size = " << levels[1].size() << std::endl;
    return bytesWritten;
}
