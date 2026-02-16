#include "SSTableBuilder.h"
#include <cstdint>

SSTableBuilder::SSTableBuilder(const std::string& path,
                               size_t bloomBits,
                               size_t bloomHashes)
    : out(path, std::ios::binary),
      bloom(bloomBits, bloomHashes),
      filePath(path) {}

void SSTableBuilder::append(const std::string& key,
                            const std::string& value){

    uint64_t offset = out.tellp();
    keyOffsets.push_back(offset);

    bloom.add(key);

    uint32_t keySize = static_cast<uint32_t>(key.size());
    uint32_t valueSize = static_cast<uint32_t>(value.size());

    out.write(reinterpret_cast<char*>(&keySize), sizeof(keySize));
    out.write(key.data(), keySize);

    out.write(reinterpret_cast<char*>(&valueSize), sizeof(valueSize));
    out.write(value.data(), valueSize);
}

void SSTableBuilder::finalize(){

    // Write bloom filter
    bloom.serialize(out);

    // Write key offsets
    uint64_t count = keyOffsets.size();
    out.write(reinterpret_cast<char*>(&count), sizeof(count));

    for (uint64_t offset : keyOffsets) {
        out.write(reinterpret_cast<char*>(&offset), sizeof(offset));
    }

    out.close();
}
