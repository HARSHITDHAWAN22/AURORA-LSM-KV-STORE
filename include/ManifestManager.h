#ifndef MANIFEST_MANAGER_H
#define MANIFEST_MANAGER_H

#include<string>
#include<vector>
#include <cstdint>
// Manages metadata required for recovery
class ManifestManager{

public:

    explicit ManifestManager(const std::string& manifestPath);

    void load();
    void save() const;
    bool levelOverflow(int level) const;

    void addSSTable(int level, const std::string& filePath,std::uint64_t fileSize);
    void removeSSTable(const std::string& filePath);
    std::uint64_t levelMaxBytes(int level) const;
    std::size_t levelFileCount(int level) const;
    const std::vector<std::vector<std::string>>& getAllSSTables() const;
    void clear();

private:
    std::string manifestPath;
    std::vector<std::vector<std::string>> sstableFilesPerLevel;
     std::vector<std::uint64_t> levelBytes;
};

#endif
// End of file: include/ManifestManager.h