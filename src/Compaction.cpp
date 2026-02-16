#include "Compaction.h"
#include <iostream>
#include <vector>
#include <ctime>
#include <algorithm>
#include "SSTableIterator.h"
#include "MergeIterator.h"
#include "MemTable.h"
#include "SSTableBuilder.h"

Compaction::Compaction(Strategy strategy, int maxFilesPerLevel)
    : strategy(strategy),
      maxFilesPerLevel(maxFilesPerLevel) {}

void Compaction::setStrategy(Strategy s) {
    strategy = s;
}

Compaction::Strategy Compaction::getStrategy() const {
    return strategy;
}

static bool rangesOverlap(const SSTable& a,
                          const SSTable& b) {
    return !(a.getMaxKey() < b.getMinKey() ||
             b.getMaxKey() < a.getMinKey());
}

void Compaction::run(std::vector<std::vector<SSTable>>& levels) {

    for (size_t level = 0; level + 1 < levels.size(); ++level) {

        if (levels[level].empty())
            continue;

        //Pick oldest file (simple + interview safe)
        SSTable& candidate = levels[level].front();

        std::vector<size_t> overlapIndexes;

        // Detect overlapping files in next level
        for (size_t i = 0; i < levels[level + 1].size(); ++i) {
            if (rangesOverlap(candidate,
                              levels[level + 1][i])) {
                overlapIndexes.push_back(i);
            }
        }

        std::cout << "Compacting Level "
                  << level
                  << " -> "
                  << (level + 1)
                  << " (overlap files: "
                  << overlapIndexes.size()
                  << ")"
                  << std::endl;

        std::vector<SSTableIterator> iters;

        // Add candidate from current level
        iters.emplace_back(candidate);

        // Add only overlapping files
        for (size_t idx : overlapIndexes) {
            iters.emplace_back(levels[level + 1][idx]);
        }

        std::vector<Iterator*> inputs;
        for (auto& it : iters)
            inputs.push_back(&it);

        MergeIterator merge(inputs);

        std::vector<std::pair<std::string,std::string>> mergedData;
        mergedData.reserve(1024);

        while (merge.valid()) {
            if (merge.value() != MemTable::TOMBSTONE) {
                mergedData.emplace_back(
                    merge.key(),
                    merge.value()
                );
            }
            merge.next();
        }

        if (mergedData.empty()) {
            // Nothing to write
            levels[level].erase(levels[level].begin());
            return;
        }

        std::string outPath =
            "data/L" + std::to_string(level + 1) + "_" +
            std::to_string(std::time(nullptr)) + ".dat";

        std::map<std::string,std::string> ordered;
        for (auto& kv : mergedData) {
            ordered.emplace(std::move(kv.first),
                            std::move(kv.second));
        }

        SSTable writer(outPath, 10000, 3);
        writer.writeToDisk(ordered);

        SSTable reloaded(outPath, 10000, 3);

        // Remove candidate from current level
        levels[level].erase(levels[level].begin());

        //  Remove only overlapping files from next level
        std::sort(overlapIndexes.rbegin(),
                  overlapIndexes.rend());

        for (size_t idx : overlapIndexes) {
            levels[level + 1].erase(
                levels[level + 1].begin() + idx
            );
        }

        //  Insert new file in sorted order (L1+ non-overlap)
        auto& nextLevel = levels[level + 1];

        nextLevel.push_back(reloaded);

        std::sort(nextLevel.begin(),
                  nextLevel.end(),
                  [](const SSTable& a,
                     const SSTable& b) {
            return a.getMinKey() < b.getMinKey();
        });

        return; // One compaction per call
    }
}
