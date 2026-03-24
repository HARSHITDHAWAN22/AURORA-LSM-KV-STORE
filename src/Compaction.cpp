#include "Compaction.h"
#include <iostream>
#include <vector>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <map>

#include "SSTableIterator.h"
#include "MemTable.h"
#include "SSTable.h"

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

// =======================
// 🔥 FINAL CORRECT COMPACTION (STRICT)
// =======================
uint64_t Compaction::run(std::vector<std::vector<SSTable>>& levels){

    for (size_t level = 0; level + 1 < levels.size(); ++level) {

        if (levels[level].empty())
            continue;

        SSTable& candidate = levels[level].front();

        std::vector<size_t> overlapIndexes;

        for (size_t i = 0; i < levels[level + 1].size(); ++i) {
            if (rangesOverlap(candidate,
                              levels[level + 1][i])) {
                overlapIndexes.push_back(i);
            }
        }

        std::cout << "Compacting Level "
                  << level << " -> " << (level + 1)
                  << " (overlap files: "
                  << overlapIndexes.size()
                  << ")" << std::endl;

        std::map<std::string, std::string> merged;

        // NEWEST FIRST
        std::vector<SSTable*> tables;
        tables.push_back(&candidate);

        for (size_t idx : overlapIndexes) {
            tables.push_back(&levels[level + 1][idx]);
        }

        for (auto* table : tables) {

            std::ifstream in(table->getFilePath(), std::ios::binary);
            if (!in.is_open()) continue;

            while (in.good()) {

                uint32_t k, v;

                if (!in.read(reinterpret_cast<char*>(&k), sizeof(k)))
                    break;

                std::string key(k, '\0');
                in.read(&key[0], k);

                if (!in.read(reinterpret_cast<char*>(&v), sizeof(v)))
                    break;

                std::string value(v, '\0');
                in.read(&value[0], v);

                // 🔥 CRITICAL FIX
                if (merged.find(key) != merged.end())
                    continue;

                if (value == MemTable::TOMBSTONE) {
                    // mark deleted (do nothing, but block older values)
                    merged[key] = MemTable::TOMBSTONE;
                } else {
                    merged[key] = value;
                }
            }
        }

        // Remove tombstones before writing
        for (auto it = merged.begin(); it != merged.end();) {
            if (it->second == MemTable::TOMBSTONE)
                it = merged.erase(it);
            else
                ++it;
        }

        if (merged.empty()) {
            levels[level].erase(levels[level].begin());
            return 0;
        }

        std::string outPath =
            "data/L" + std::to_string(level + 1) + "_" +
            std::to_string(std::time(nullptr)) + ".dat";

        SSTable writer(outPath, 10000, 3);
        writer.writeToDisk(merged);

        SSTable reloaded(outPath, 10000, 3);
        uint64_t compactionBytes = reloaded.getFileSize();

        levels[level].erase(levels[level].begin());

        std::sort(overlapIndexes.rbegin(),
                  overlapIndexes.rend());

        for (size_t idx : overlapIndexes) {
            levels[level + 1].erase(
                levels[level + 1].begin() + idx
            );
        }

        auto& nextLevel = levels[level + 1];

        nextLevel.push_back(reloaded);

        std::sort(nextLevel.begin(),
                  nextLevel.end(),
                  [](const SSTable& a,
                     const SSTable& b){
            return a.getMinKey() < b.getMinKey();
        });

        return compactionBytes;
    }

    return 0;
}