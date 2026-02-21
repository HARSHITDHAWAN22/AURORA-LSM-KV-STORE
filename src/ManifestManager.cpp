#include "ManifestManager.h"
#include "Logger.h"

#include <fstream>
#include <filesystem>
#include <sstream>

ManifestManager::ManifestManager(const std::string& manifestPath)
    : manifestPath(manifestPath) {

    levels.resize(4);   // MAX_LEVELS
    levelBytes.resize(4, 0ULL);

    LOG_INFO("ManifestManager initialized at path: " + manifestPath);
}

void ManifestManager::load() {

    std::ifstream in(manifestPath);

    if (!in.is_open()) {
        LOG_DEBUG("Manifest load skipped: file not found");
        return;
    }

    levels.clear();
    levels.resize(4);

    levelBytes.clear();
    levelBytes.resize(4, 0ULL);

    std::string line;
    int currentLevel = -1;

    while (std::getline(in, line)) {

        if (line.rfind("LEVEL:", 0) == 0) {
            currentLevel = std::stoi(line.substr(6));
            continue;
        }

        if (!line.empty() && currentLevel >= 0 && currentLevel < 4) {

            std::stringstream ss(line);
            std::string path, minK, maxK, sizeStr;

            std::getline(ss, path, '|');
            std::getline(ss, minK, '|');
            std::getline(ss, maxK, '|');
            std::getline(ss, sizeStr, '|');

            if (path.empty() || sizeStr.empty()) {
                LOG_ERROR("Manifest load error: corrupted line detected");
                continue;
            }

            std::uint64_t fileSize = std::stoull(sizeStr);

            SSTableMeta meta(path, minK, maxK, fileSize);

            levels[currentLevel].push_back(meta);
            levelBytes[currentLevel] += fileSize;
        }
    }

    LOG_INFO("Manifest loaded successfully");
}

void ManifestManager::save() const {

    std::string tempPath = manifestPath + ".tmp";

    std::ofstream out(tempPath, std::ios::trunc);

    if (!out.is_open()) {
        LOG_ERROR("Manifest save failed: unable to open temp file");
        return;
    }

    for (int level = 0; level < (int)levels.size(); ++level) {

        out << "LEVEL:" << level << "\n";

        for (const auto& meta : levels[level]) {
            out << meta.filePath << "|"
                << meta.minKey << "|"
                << meta.maxKey << "|"
                << meta.fileSize << "\n";
        }
    }

    out.close();

    std::error_code ec;
    std::filesystem::rename(tempPath, manifestPath, ec);

    if (ec) {
        LOG_ERROR("Manifest rename failed: " + ec.message());
        return;
    }

    LOG_DEBUG("Manifest saved successfully");
}

void ManifestManager::addSSTable(int level,
                                 const SSTableMeta& meta) {

    if (level < 0 || level >= (int)levels.size()) {
        LOG_ERROR("Manifest addSSTable: invalid level");
        return;
    }

    levels[level].push_back(meta);
    levelBytes[level] += meta.fileSize;

    LOG_DEBUG("SSTable added to manifest at level " + std::to_string(level));
}

void ManifestManager::removeSSTable(const std::string& filePath) {

    for (int level = 0; level < (int)levels.size(); ++level) {

        auto& vec = levels[level];

        for (auto it = vec.begin(); it != vec.end(); ++it) {

            if (it->filePath == filePath) {
                levelBytes[level] -= it->fileSize;
                vec.erase(it);

                LOG_DEBUG("SSTable removed from manifest: " + filePath);
                return;
            }
        }
    }
}

std::uint64_t ManifestManager::levelMaxBytes(int level) const {

    const std::uint64_t base = 10ULL * 1024ULL * 1024ULL; // 10MB

    if (level == 0)
        return base;

    std::uint64_t size = base;
    for (int i = 0; i < level; ++i)
        size *= 10ULL;

    return size;
}

bool ManifestManager::levelOverflow(int level) const {

    if (level < 0 || level >= (int)levels.size())
        return false;

    if (level == 0)
        return levels[0].size() > 4;

    return levelBytes[level] > levelMaxBytes(level);
}

std::uint64_t ManifestManager::levelBytesUsed(int level) const {

    if (level < 0 || level >= (int)levelBytes.size())
        return 0;

    return levelBytes[level];
}

std::size_t ManifestManager::levelFileCount(int level) const {

    if (level < 0 || level >= (int)levels.size())
        return 0;

    return levels[level].size();
}

const std::vector<std::vector<SSTableMeta>>&
ManifestManager::getLevels() const {
    return levels;
}

void ManifestManager::clear() {

    for (auto& v : levels)
        v.clear();

    std::fill(levelBytes.begin(),
              levelBytes.end(),
              0ULL);

    LOG_DEBUG("Manifest cleared");
}