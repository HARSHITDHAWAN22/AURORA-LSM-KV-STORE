#include "SSTable.h"
#include <fstream>
#include <vector>
#include <iostream>
#include "MemTable.h"

struct SSTableFooter {
    uint64_t indexOffset;
    uint64_t magic;
};

SSTable::SSTable(const std::string& filePath,
                 size_t bloomBitSize,
                 size_t bloomHashCount)
    : filePath(filePath),
      bloom(bloomBitSize, bloomHashCount) {

    // If file does not exist, just return (normal for new flush)
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open())
        return;

    // Only rebuild bloom if valid SSTable
    if (!isBinarySSTable())
        return;

    // Read footer
    in.seekg(-static_cast<std::streamoff>(sizeof(SSTableFooter)), std::ios::end);
    SSTableFooter footer;
    in.read(reinterpret_cast<char*>(&footer), sizeof(footer));

    if (footer.magic != SSTABLE_MAGIC)
        return;

    uint64_t dataEnd = footer.indexOffset;
    in.seekg(0);

    // Rebuild bloom from data section only
    while (in.good() &&
           static_cast<uint64_t>(in.tellg()) < dataEnd) {

        uint32_t k, v;

        if (!in.read(reinterpret_cast<char*>(&k), sizeof(k)))
            break;

        std::string key(k, '\0');
        in.read(&key[0], k);

        if (!in.read(reinterpret_cast<char*>(&v), sizeof(v)))
            break;

        // Skip value (no need to load it)
        in.seekg(v, std::ios::cur);

        bloom.add(key);
    }
}

bool SSTable::isBinarySSTable() const {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open())
        return false;

    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();

    if (size < static_cast<std::streamoff>(sizeof(SSTableFooter)))
        return false;

    in.seekg(-static_cast<std::streamoff>(sizeof(SSTableFooter)), std::ios::end);

    SSTableFooter footer;
    in.read(reinterpret_cast<char*>(&footer), sizeof(footer));

    return footer.magic == SSTABLE_MAGIC;
}

bool SSTable::writeToDisk(const std::map<std::string, std::string>& data) {

    // Reset bloom before writing
   bloom = BloomFilter(10000, 3);   // use same values used in constructor


    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        return false;

    constexpr size_t INDEX_INTERVAL = 64;
    sparseIndex.clear();

    for (const auto& entry : data) {

        uint64_t offset = static_cast<uint64_t>(out.tellp());

        if (sparseIndex.size() % INDEX_INTERVAL == 0) {
            sparseIndex.emplace_back(entry.first, offset);
        }

        uint32_t k = static_cast<uint32_t>(entry.first.size());
        uint32_t v = static_cast<uint32_t>(entry.second.size());

        out.write(reinterpret_cast<char*>(&k), sizeof(k));
        out.write(entry.first.data(), k);
        out.write(reinterpret_cast<char*>(&v), sizeof(v));
        out.write(entry.second.data(), v);

        bloom.add(entry.first);
    }

    uint64_t indexOffset = static_cast<uint64_t>(out.tellp());
    uint32_t indexCount = static_cast<uint32_t>(sparseIndex.size());

    out.write(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));

    for (const auto& e : sparseIndex) {
        uint32_t k = static_cast<uint32_t>(e.key.size());

        out.write(reinterpret_cast<char*>(&k), sizeof(k));
        out.write(e.key.data(), k);
        out.write(reinterpret_cast<const char*>(&e.offset), sizeof(e.offset));
    }

    SSTableFooter footer{ indexOffset, SSTABLE_MAGIC };
    out.write(reinterpret_cast<char*>(&footer), sizeof(footer));

    out.flush();
    return true;
}

GetResult SSTable::get(const std::string& key,
                       std::string& value) const {
    return getBinary(key, value);
}

GetResult SSTable::getBinary(const std::string& key,
                             std::string& value) const {

    if (!bloom.mightContain(key))
        return GetResult::NOT_FOUND;

    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open())
        return GetResult::NOT_FOUND;

    in.seekg(-static_cast<std::streamoff>(sizeof(SSTableFooter)), std::ios::end);
    SSTableFooter footer;
    in.read(reinterpret_cast<char*>(&footer), sizeof(footer));

    if (footer.magic != SSTABLE_MAGIC)
        return GetResult::NOT_FOUND;

    in.seekg(footer.indexOffset);

    uint32_t indexCount;
    in.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));

    std::vector<SSTableIndexEntry> localIndex;
    localIndex.reserve(indexCount);

    for (uint32_t i = 0; i < indexCount; i++) {

        uint32_t k;
        uint64_t off;

        in.read(reinterpret_cast<char*>(&k), sizeof(k));

        std::string ik(k, '\0');
        in.read(&ik[0], k);

        in.read(reinterpret_cast<char*>(&off), sizeof(off));

        localIndex.emplace_back(ik, off);
    }

    uint64_t startOffset = 0;

    for (const auto& e : localIndex) {
        if (e.key <= key)
            startOffset = e.offset;
        else
            break;
    }

    in.seekg(static_cast<std::streamoff>(startOffset));

    while (in.good() &&
           static_cast<uint64_t>(in.tellg()) < footer.indexOffset) {

        uint32_t k, v;

        if (!in.read(reinterpret_cast<char*>(&k), sizeof(k)))
            break;

        std::string curKey(k, '\0');
        in.read(&curKey[0], k);

        if (!in.read(reinterpret_cast<char*>(&v), sizeof(v)))
            break;

        std::string curVal(v, '\0');
        in.read(&curVal[0], v);

        if (curKey == key) {

            if (curVal == MemTable::TOMBSTONE)
                return GetResult::DELETED;

            value = curVal;
            return GetResult::FOUND;
        }

        if (curKey > key)
            break;
    }

    return GetResult::NOT_FOUND;
}

const std::string& SSTable::getFilePath() const {
    return filePath;
}
