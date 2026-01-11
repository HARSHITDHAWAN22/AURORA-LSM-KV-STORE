#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>

class ConfigManager{
public:
    explicit ConfigManager(const std::string& configPath);

    // Load configuration from JSON file
    bool load();

    // Getters
    int getMemTableMaxEntries() const;
    int getBloomFilterBitSize() const;
    int getBloomFilterHashCount() const;
    int getMaxFilesPerLevel() const;
    std::string getSSTableDirectory() const;

private:
    std::string configFilePath;

    // Stored configuration values
    int memTableMaxEntries;
    int bloomFilterBitSize;
    int bloomFilterHashCount;
    int maxFilesPerLevel;
    std::string sstableDirectory;
};

#endif
