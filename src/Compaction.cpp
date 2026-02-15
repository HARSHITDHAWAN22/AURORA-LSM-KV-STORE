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

    for (size_t level = 0; level + 1 < levels.size(); ++level) {

        const size_t L0_TRIGGER = 4;

        if (level == 0 && levels[0].size() <= L0_TRIGGER)
            continue;

        if (levels[level].empty())
            continue;

        std::cout << "Compacting Level "
                  << level
                  << " -> "
                  << (level + 1)
                  << std::endl;

        // Move ALL files of this level upward
        size_t filesToCompact = levels[level].size();

        std::vector<SSTableIterator> iters;
        iters.reserve(filesToCompact);

        for (size_t i = 0; i < filesToCompact; ++i) {
            iters.emplace_back(levels[level][i]);
        }

        std::vector<Iterator*> inputs;
        inputs.reserve(filesToCompact);

        for (auto& it : iters)
            inputs.push_back(&it);

        MergeIterator merge(inputs);

        std::string outPath =
            "data/L" + std::to_string(level + 1) + "_" +
            std::to_string(std::time(nullptr)) + ".dat";

        SSTable writer(outPath, 10000, 3);

        std::map<std::string, std::string> buffer;

        const size_t FLUSH_THRESHOLD = 5000;

        while (merge.valid()) {

            if (merge.value() != MemTable::TOMBSTONE) {
                buffer[merge.key()] = merge.value();
            }

            if (buffer.size() >= FLUSH_THRESHOLD) {
                writer.writeToDisk(buffer);
                buffer.clear();
            }

            merge.next();
        }

        if (!buffer.empty())
            writer.writeToDisk(buffer);

        SSTable reloaded(outPath, 10000, 3);

        if (levels.size() <= level + 1)
            levels.resize(level + 2);

        levels[level + 1].push_back(reloaded);

        // Clear old level (safe memory cleanup)
        levels[level].clear();
    }
}
