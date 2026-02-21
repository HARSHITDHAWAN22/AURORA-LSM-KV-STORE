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
      maxFilesPerLevel(0),
      compactionIntervalSeconds(5),
      l0Threshold(4),
      flushIntervalSeconds(2)
{}

bool ConfigManager::load(){

    ifstream file(configFilePath);
    if(!file.is_open()){
        cerr << "Failed to open config file: "
             << configFilePath << endl;
        return false;
    }

    json config;
    file >> config;

    // Safe defaults
    compactionIntervalSeconds = 5;
    l0Threshold = 4;
    flushIntervalSeconds = 2;

    memTableMaxEntries =
        config["storage"]["memtable"]["max_entries"];

    sstableDirectory =
        config["storage"]["sstable"]["data_directory"];

    if(sstableDirectory.empty()){
        cerr << "Invalid SSTable directory in config\n";
        return false;
    }

    std::filesystem::create_directories(sstableDirectory);

    bloomFilterBitSize =
        config["bloom_filter"]["bit_size"];

    bloomFilterHashCount =
        config["bloom_filter"]["hash_functions"];

    maxFilesPerLevel =
        config["compaction"]["max_files_per_level"];

    // ---- Optional Advanced Fields ----

    if(config.contains("compaction")){
        auto comp = config["compaction"];

        if (comp.contains("interval_seconds"))
            compactionIntervalSeconds =
                comp["interval_seconds"];

        if (comp.contains("l0_threshold"))
            l0Threshold =
                comp["l0_threshold"];
    }

    if(config.contains("flush")){
        auto fl = config["flush"];

        if (fl.contains("interval_seconds"))
            flushIntervalSeconds =
                fl["interval_seconds"];
    }

    return true;
}

int ConfigManager::getMemTableMaxEntries() const{
    return memTableMaxEntries;
}

int ConfigManager::getBloomFilterBitSize() const {
    return bloomFilterBitSize;
}

int ConfigManager::getBloomFilterHashCount() const{
    return bloomFilterHashCount;
}

int ConfigManager::getMaxFilesPerLevel() const{
    return maxFilesPerLevel;
}

std::string ConfigManager::getSSTableDirectory() const{
    return sstableDirectory;
}

int ConfigManager::getCompactionInterval() const{
    return compactionIntervalSeconds;
}

int ConfigManager::getL0Threshold() const{
    return l0Threshold;
}

int ConfigManager::getFlushInterval() const{
    return flushIntervalSeconds;
}