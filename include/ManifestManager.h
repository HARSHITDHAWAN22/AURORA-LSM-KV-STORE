#ifndef MANIFEST_MANAGER_H
#define MANIFEST_MANAGER_H

#include<string>
#include<vector>

// Manages metadata required for recovery
class ManifestManager{

public:

    explicit ManifestManager(const std::string& manifestPath);

    void load();
    void save() const;

    void addSSTable(int level, const std::string& filePath);
    const std::vector<std::vector<std::string>>& getAllSSTables() const;
    void clear();

private:
    std::string manifestPath;
    std::vector<std::vector<std::string>> sstableFilesPerLevel;
};

#endif
// End of file: include/ManifestManager.h