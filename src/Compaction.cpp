#include "Compaction.h"
#include <map>
#include <iostream>
#include <vector>
#include <ctime>
#include "SSTableIterator.h"
#include "MergeIterator.h"
#include "MemTable.h"

Compaction::Compaction(Strategy strategy, int maxFilesPerLevel)
    : strategy(strategy),
      maxFilesPerLevel(maxFilesPerLevel) {}

void Compaction::setStrategy(Strategy s) {
    strategy = s;
}

Compaction::Strategy Compaction::getStrategy() const {
    return strategy;
}

void Compaction::run(std::vector<std::vector<SSTable>>& levels) {

    if (levels.empty()) return;

    size_t level = 0;

    const size_t L0_TRIGGER = 4;          // compact when >4 files
    const size_t COMPACT_BATCH = 4;       // compact only 4 files

    if (levels[level].size() <= L0_TRIGGER)
        return;

    std::cout << "Compacting L0 -> L1\n";

    size_t filesToCompact =
        std::min(COMPACT_BATCH, levels[level].size());

    std::vector<SSTableIterator> iters;
    iters.reserve(filesToCompact);

    for (size_t i = 0; i < filesToCompact; ++i) {
        iters.emplace_back(levels[level][i]);
    }

    std::vector<Iterator*> inputs;
    inputs.reserve(filesToCompact);

    for (auto& it : iters) {
        inputs.push_back(&it);
    }

    MergeIterator merge(inputs);

    std::map<std::string, std::string> mergedData;

    while (merge.valid()) {
        if (merge.value() != MemTable::TOMBSTONE) {
            mergedData[merge.key()] = merge.value();
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

    if (levels.size() < 2)
        levels.resize(2);

    levels[1].push_back(reloaded);

    // Remove ONLY compacted files
    levels[0].erase(
        levels[0].begin(),
        levels[0].begin() + filesToCompact
    );
}
