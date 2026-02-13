#ifndef COMPACTION_H
#define COMPACTION_H

#include <vector>
#include "SSTable.h"


// Handles SSTable merging and cleanup
class Compaction{
public:
    enum class Strategy{
        LEVEL,
        TIERED
    };


    Compaction(Strategy strategy, int maxFilesPerLevel);
    void setStrategy(Strategy s);
Strategy getStrategy() const;


    // Run compaction on current SSTables
    void run(std::vector<SSTable>& sstables);

private:

    void runLevelCompaction(std::vector<SSTable>& sstables);
    void runTieredCompaction(std::vector<SSTable>& sstables);

private:

    Strategy strategy;
    int maxFilesPerLevel;
};

#endif
