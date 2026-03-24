#include "SSTable.h"
#include "Logger.h"

#include <fstream>
#include <vector>
#include "MemTable.h"

struct SSTableFooter {
    uint64_t indexOffset;
    uint64_t minKeyOffset;
    uint64_t maxKeyOffset;
    uint64_t fileSize;
    uint64_t magic;
};

// BLOCK SIZE (NEW)
constexpr size_t BLOCK_SIZE = 4;

// =======================
// Constructor 
// =======================
SSTable::SSTable(const std::string& filePath,
                 size_t bloomBitSize,
                 size_t bloomHashCount)
    : filePath(filePath),
      bloom(bloomBitSize, bloomHashCount) {

    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open())
        return;

    if (!isBinarySSTable()) {
        LOG_ERROR("Invalid SSTable format: " + filePath);
        return;
    }

    loadFooterMetadata();

    uint64_t indexOffset = 0;
    {
        std::ifstream fin(filePath, std::ios::binary);
        fin.seekg(-static_cast<std::streamoff>(sizeof(SSTableFooter)), std::ios::end);

        SSTableFooter footer;
        fin.read(reinterpret_cast<char*>(&footer), sizeof(footer));
        indexOffset = footer.indexOffset;
    }

    in.seekg(0);

    while (in.good() &&
           static_cast<uint64_t>(in.tellg()) < indexOffset) {

        uint32_t k, v;

        if (!in.read(reinterpret_cast<char*>(&k), sizeof(k)))
            break;

        std::string key(k, '\0');
        in.read(&key[0], k);

        if (!in.read(reinterpret_cast<char*>(&v), sizeof(v)))
            break;

        in.seekg(v, std::ios::cur);

        bloom.add(key);
    }
}

// =======================
void SSTable::loadFooterMetadata() {

    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open())
        return;

    in.seekg(-static_cast<std::streamoff>(sizeof(SSTableFooter)), std::ios::end);

    SSTableFooter footer;
    in.read(reinterpret_cast<char*>(&footer), sizeof(footer));

    if (footer.magic != SSTABLE_MAGIC) {
        LOG_ERROR("SSTable footer magic mismatch: " + filePath);
        return;
    }

    fileSize = footer.fileSize;

    in.seekg(footer.minKeyOffset);
    uint32_t minSize;
    in.read(reinterpret_cast<char*>(&minSize), sizeof(minSize));
    minKey.resize(minSize);
    in.read(&minKey[0], minSize);

    in.seekg(footer.maxKeyOffset);
    uint32_t maxSize;
    in.read(reinterpret_cast<char*>(&maxSize), sizeof(maxSize));
    maxKey.resize(maxSize);
    in.read(&maxKey[0], maxSize);
}

// =======================
bool SSTable::mightContain(const std::string& key) const {

    if (statsHook) statsHook->recordBloomCheck();

    bool result = bloom.mightContain(key);

    if (!result && statsHook) statsHook->recordBloomNegative();

    return result;
}

// =======================
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

// =======================
//UPDATED: BLOCK INDEX WRITE
// =======================
bool SSTable::writeToDisk(const std::map<std::string, std::string>& data) {

    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        LOG_ERROR("Failed to create SSTable: " + filePath);
        return false;
    }

    sparseIndex.clear();

    std::string localMinKey;
    std::string localMaxKey;
    bool first = true;

    size_t entryCount = 0;

    for (const auto& entry : data) {

        if (first) {
            localMinKey = entry.first;
            first = false;
        }
        localMaxKey = entry.first;

        // BLOCK INDEX
        if (entryCount % BLOCK_SIZE == 0) {
            uint64_t offset = static_cast<uint64_t>(out.tellp());
            sparseIndex.emplace_back(entry.first, offset);
        }

        entryCount++;

        uint32_t k = entry.first.size();
        uint32_t v = entry.second.size();

        out.write(reinterpret_cast<char*>(&k), sizeof(k));
        out.write(entry.first.data(), k);
        out.write(reinterpret_cast<char*>(&v), sizeof(v));
        out.write(entry.second.data(), v);

        bloom.add(entry.first);
    }

    uint64_t indexOffset = static_cast<uint64_t>(out.tellp());
    uint32_t indexCount = sparseIndex.size();

    out.write(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));

    for (const auto& e : sparseIndex) {
        uint32_t k = e.key.size();
        out.write(reinterpret_cast<char*>(&k), sizeof(k));
        out.write(e.key.data(), k);
        out.write(reinterpret_cast<const char*>(&e.offset), sizeof(e.offset));
    }

    uint64_t minKeyOffset = static_cast<uint64_t>(out.tellp());
    uint32_t minSize = localMinKey.size();
    out.write(reinterpret_cast<char*>(&minSize), sizeof(minSize));
    out.write(localMinKey.data(), minSize);

    uint64_t maxKeyOffset = static_cast<uint64_t>(out.tellp());
    uint32_t maxSize = localMaxKey.size();
    out.write(reinterpret_cast<char*>(&maxSize), sizeof(maxSize));
    out.write(localMaxKey.data(), maxSize);

    uint64_t finalSize = static_cast<uint64_t>(out.tellp());

    SSTableFooter footer{
        indexOffset,
        minKeyOffset,
        maxKeyOffset,
        finalSize,
        SSTABLE_MAGIC
    };

    out.write(reinterpret_cast<char*>(&footer), sizeof(footer));

    out.close();
    return true;
}

// =======================
// UPDATED: BLOCK LIMITED SEARCH
// =======================
GetResult SSTable::getBinary(const std::string& key,
                             std::string& value) const {

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

    //Binary search
    int left = 0, right = localIndex.size() - 1;
    uint64_t startOffset = 0;

    while (left <= right) {
        int mid = (left + right) / 2;
        if (localIndex[mid].key <= key) {
            startOffset = localIndex[mid].offset;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    //Find next block boundary
    uint64_t nextBlockOffset = footer.indexOffset;
    for (const auto& e : localIndex) {
        if (e.offset > startOffset) {
            nextBlockOffset = e.offset;
            break;
        }
    }

    in.seekg(startOffset);

    while (in.good() &&
           static_cast<uint64_t>(in.tellg()) < nextBlockOffset) {

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

    if (statsHook) statsHook->recordBloomFalsePositive();

    return GetResult::NOT_FOUND;
}

// =======================
GetResult SSTable::get(const std::string& key,
                       std::string& value) const {

    if (!minKey.empty() && (key < minKey || key > maxKey))
        return GetResult::NOT_FOUND;

    return getBinary(key, value);
}

// =======================
const std::string& SSTable::getFilePath() const {
    return filePath;
}