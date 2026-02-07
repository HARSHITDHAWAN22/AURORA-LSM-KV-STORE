#include "SSTable.h"
#include <fstream>
#include <vector>
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

    loadBloom();

    if (!isBinarySSTable()) {
        loadSparseIndex();
    }
}

void SSTable::loadBloom() {
    if (isBinarySSTable()) return;

    std::ifstream in(filePath);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        if (line == "#INDEX") break;

        auto pos = line.find('\t');
        if (pos == std::string::npos) continue;

        bloom.add(line.substr(0, pos));
    }
}

bool SSTable::isBinarySSTable() const {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) return false;

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
    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open()) return false;

    constexpr size_t INDEX_INTERVAL = 64;
    sparseIndex.clear();

    // ---- DATA SECTION ----
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

    // ---- INDEX SECTION ----
    uint64_t indexOffset = static_cast<uint64_t>(out.tellp());
    uint32_t indexCount = static_cast<uint32_t>(sparseIndex.size());

    out.write(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
    for (const auto& e : sparseIndex) {
        uint32_t k = static_cast<uint32_t>(e.key.size());
        out.write(reinterpret_cast<char*>(&k), sizeof(k));
        out.write(e.key.data(), k);
        out.write(reinterpret_cast<const char*>(&e.offset), sizeof(e.offset));
    }

    // ---- FOOTER ----
    SSTableFooter footer{indexOffset, SSTABLE_MAGIC};
    out.write(reinterpret_cast<char*>(&footer), sizeof(footer));

    out.flush();
    return true;
}

bool SSTable::get(const std::string& key, std::string& value) const {
    if (isBinarySSTable())
        return getBinary(key, value);

    if (!bloom.mightContain(key)) return false;

    std::ifstream in(filePath);
    if (!in.is_open()) return false;

    size_t startLine = 0;
    size_t l = 0, r = sparseIndex.size();

    while (l < r) {
        size_t mid = (l + r) / 2;
        if (sparseIndex[mid].key <= key) {
            startLine = static_cast<size_t>(sparseIndex[mid].offset);
            l = mid + 1;
        } else {
            r = mid;
        }
    }

    std::string line;
    for (size_t i = 0; i < startLine && std::getline(in, line); i++);

    while (std::getline(in, line)) {
        if (line == "#INDEX") break;

        auto pos = line.find('\t');
        if (pos == std::string::npos) continue;

        std::string currentKey = line.substr(0, pos);
        if (currentKey == key) {
            value = line.substr(pos + 1);
            return true;
        }
        if (currentKey > key) break;
    }

    return false;
}

bool SSTable::getBinary(const std::string& key, std::string& value) const {
    if (!bloom.mightContain(key)) return false;

    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) return false;

    // ---- read footer ----
    in.seekg(-static_cast<std::streamoff>(sizeof(SSTableFooter)), std::ios::end);
    SSTableFooter footer;
    in.read(reinterpret_cast<char*>(&footer), sizeof(footer));

    if (footer.magic != SSTABLE_MAGIC) return false;

    // ---- load index (LOCAL ONLY) ----
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

    // ---- binary search index ----
    uint64_t startOffset = 0;
    for (const auto& e : localIndex) {
        if (e.key <= key) startOffset = e.offset;
        else break;
    }

    // ---- data scan ----
    in.seekg(static_cast<std::streamoff>(startOffset));
    while (in.good()) {
        uint32_t k, v;
        if (!in.read(reinterpret_cast<char*>(&k), sizeof(k))) break;

        std::string curKey(k, '\0');
        in.read(&curKey[0], k);

        in.read(reinterpret_cast<char*>(&v), sizeof(v));
        std::string curVal(v, '\0');
        in.read(&curVal[0], v);

        if (curKey == key) {
             
    
            value = curVal;
            return true;
        }
        if (curKey > key) break;
    }

    return false;
}

void SSTable::loadSparseIndex() {
    if (isBinarySSTable()) return;

    std::ifstream in(filePath);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        if (line == "#INDEX") break;
    }

    if (!in.good()) return;

    size_t count = 0;
    in >> count;
    in.ignore();

    sparseIndex.clear();
    for (size_t i = 0; i < count; i++) {
        std::string key;
        uint64_t lineNo;
        in >> key >> lineNo;
        in.ignore();
        sparseIndex.emplace_back(key, lineNo);
    }
}

const std::string& SSTable::getFilePath() const {
    return filePath;
}
