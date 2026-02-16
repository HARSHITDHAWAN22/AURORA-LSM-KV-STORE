#ifndef SSTABLE_BUILDER_H
#define SSTABLE_BUILDER_H

#include <fstream>
#include <string>
#include <vector>
#include "BloomFilter.h"
#include <cstdint>


class SSTableBuilder{

public:
    SSTableBuilder(const std::string& path,
                   size_t bloomBits,
                   size_t bloomHashes);

    void append(const std::string& key,
                const std::string& value);

    void finalize();


private:

    std::ofstream out;
    BloomFilter bloom;
    std::vector<uint64_t> keyOffsets;
    std::string filePath;
};

#endif


