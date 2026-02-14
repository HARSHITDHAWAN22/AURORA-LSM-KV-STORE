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


    // Run compaction on current SSTables, returns bytes written
    size_t run(std::vector<std::vector<SSTable>>& levels);

private:
static constexpr size_t L0_THRESHOLD = 3;


    

private:

    Strategy strategy;
    int maxFilesPerLevel;
};

#endif
