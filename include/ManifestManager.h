#ifndef MANIFEST_MANAGER_H
#define MANIFEST_MANAGER_H

#include <string>
#include <vector>
#include <cstdint>

// Metadata for each SSTable
struct SSTableMeta {
    std::string filePath;
    std::string minKey;
    std::string maxKey;
    std::uint64_t fileSize;

    SSTableMeta() = default;

    SSTableMeta(const std::string& path,
                 const std::string& minK,
                 const std::string& maxK,
                 std::uint64_t size)
        : filePath(path),
          minKey(minK),
          maxKey(maxK),
          fileSize(size) {}
};

// Manages metadata required for recovery
class ManifestManager {

public:
    explicit ManifestManager(const std::string& manifestPath);

    void load();
    void save() const;

    bool levelOverflow(int level) const;

    // UPDATED
    void addSSTable(int level,
                    const SSTableMeta& meta);

    void removeSSTable(const std::string& filePath);

    std::uint64_t levelMaxBytes(int level) const;
    std::uint64_t levelBytesUsed(int level) const;

    std::size_t levelFileCount(int level) const;

    const std::vector<std::vector<SSTableMeta>>& getLevels() const;

    void clear();

private:
    std::string manifestPath;

    // FULL metadata per level
    std::vector<std::vector<SSTableMeta>> levels;

    // Level size tracking
    std::vector<std::uint64_t> levelBytes;
};

#endif
