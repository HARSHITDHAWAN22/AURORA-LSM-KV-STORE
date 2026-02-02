#include "SSTable.h"
#include <fstream>

SSTable::SSTable(const std::string& filePath,
                 size_t bloomBitSize,
                 size_t bloomHashCount)
    : filePath(filePath),
      bloom(bloomBitSize, bloomHashCount) {
    loadBloom();
    loadSparseIndex();
}

void SSTable::loadBloom() {
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

bool SSTable::writeToDisk(const std::map<std::string, std::string>& data) {
    std::ofstream out(filePath);
    if (!out.is_open()) return false;

    constexpr size_t INDEX_INTERVAL = 64;

    sparseIndex.clear();
    size_t lineNo = 0;

    // write data + build sparse index
    for (const auto& entry : data) {
        if (lineNo % INDEX_INTERVAL == 0) {
            sparseIndex.emplace_back(entry.first, lineNo);
        }

        out << entry.first << "\t" << entry.second << "\n";
        bloom.add(entry.first);
        lineNo++;
    }

    // persist sparse index
    out << "#INDEX\n";
    out << sparseIndex.size() << "\n";
    for (const auto& e : sparseIndex) {
        out << e.key << " " << e.offset << "\n";
    }
    out << "#END\n";

    return true;
}

bool SSTable::get(const std::string& key, std::string& value) const {
    if (!bloom.mightContain(key)) return false;

    std::ifstream in(filePath);
    if (!in.is_open()) return false;

    // binary search sparse index
    size_t startLine = 0;
    size_t l = 0, r = sparseIndex.size();

    while (l < r) {
        size_t mid = (l + r) / 2;
        if (sparseIndex[mid].key <= key) {
            startLine = sparseIndex[mid].offset;
            l = mid + 1;
        } else {
            r = mid;
        }
    }

    std::string line;

    // skip lines
    for (size_t i = 0; i < startLine && std::getline(in, line); i++);

    // bounded scan
    while (std::getline(in, line)) {
        if (line == "#INDEX") break;

        auto pos = line.find('\t');
        if (pos == std::string::npos) continue;

        std::string currentKey = line.substr(0, pos);
        if (currentKey == key) {
            value = line.substr(pos + 1);
            return true;
        }
        if (currentKey > key) {
            break;
        }
    }

    return false;
}

void SSTable::loadSparseIndex() {
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
