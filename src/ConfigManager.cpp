#include "ConfigManager.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

ConfigManager::ConfigManager(const std::string& configPath)
    : configFilePath(configPath),
      memTableMaxEntries(0),
      bloomFilterBitSize(0),
      bloomFilterHashCount(0),
      maxFilesPerLevel(0) {}

bool ConfigManager::load(){
    ifstream file(configFilePath);
    if(!file.is_open()){
        cerr << "Failed to open config file: " << configFilePath << endl;
        return false;
    }

    json config;
    file >> config;

    memTableMaxEntries =
        config["storage"]["memtable"]["max_entries"];

    sstableDirectory =
        config["storage"]["sstable"]["data_directory"];

    if(sstableDirectory.empty()){
        cerr << "Invalid SSTable directory in config\n";
        return false;
    }

    
    // ensure directory exists
    std::filesystem::create_directories(sstableDirectory);

    bloomFilterBitSize =
        config["bloom_filter"]["bit_size"];

    bloomFilterHashCount =
        config["bloom_filter"]["hash_functions"];

    maxFilesPerLevel =
        config["compaction"]["max_files_per_level"];

    return true;
}

int ConfigManager::getMemTableMaxEntries() const{
    return memTableMaxEntries;
}

int ConfigManager::getBloomFilterBitSize() const{
    return bloomFilterBitSize;
}

int ConfigManager::getBloomFilterHashCount() const{
    return bloomFilterHashCount;
}

int ConfigManager::getMaxFilesPerLevel() const{
    return maxFilesPerLevel;
}

string ConfigManager::getSSTableDirectory() const{
    return sstableDirectory;
}
