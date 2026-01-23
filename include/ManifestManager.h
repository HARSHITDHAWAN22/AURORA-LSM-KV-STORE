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

    void addSSTable(const std::string& filePath);
    const std::vector<std::string>& getSSTables() const;

private:
    std::string manifestPath;
    std::vector<std::string> sstableFiles;
};

#endif
// End of file: include/ManifestManager.h