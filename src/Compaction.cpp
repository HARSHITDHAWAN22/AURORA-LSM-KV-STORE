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

    const size_t L0_BATCH = 4;
    const size_t HIGHER_BATCH = 1;

    for (size_t level = 0; level + 1 < levels.size(); ++level) {

        if (levels[level].empty())
            continue;

        size_t batchSize = (level == 0)
                           ? std::min(L0_BATCH, levels[level].size())
                           : std::min(HIGHER_BATCH, levels[level].size());

        if (batchSize == 0)
            continue;

        std::cout << "Compacting Level "
                  << level
                  << " -> "
                  << (level + 1)
                  << std::endl;

        std::vector<SSTableIterator> iters;
        for (size_t i = 0; i < batchSize; ++i) {
            iters.emplace_back(levels[level][i]);
        }

        std::vector<Iterator*> inputs;
        for (auto& it : iters)
            inputs.push_back(&it);

        MergeIterator merge(inputs);

        std::map<std::string, std::string> mergedData;

        while (merge.valid()) {
            if (merge.value() != MemTable::TOMBSTONE) {
                mergedData[merge.key()] = merge.value();
            }
            merge.next();
        }

        std::string outPath =
            "data/L" + std::to_string(level + 1) + "_" +
            std::to_string(std::time(nullptr)) + ".dat";

        SSTable writer(outPath, 10000, 3);
        writer.writeToDisk(mergedData);

        SSTable reloaded(outPath, 10000, 3);
        levels[level + 1].push_back(reloaded);

        // Remove ONLY compacted files
        levels[level].erase(
            levels[level].begin(),
            levels[level].begin() + batchSize
        );

        return; // stop after one compaction
    }
}
