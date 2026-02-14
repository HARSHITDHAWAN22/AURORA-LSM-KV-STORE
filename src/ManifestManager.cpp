#include "ManifestManager.h"
#include <fstream>
#include <filesystem>

ManifestManager::ManifestManager(const std::string& manifestPath)
    : manifestPath(manifestPath){
    sstableFilesPerLevel.clear();
    sstableFilesPerLevel.resize(4); // MAX_LEVELS
}

void ManifestManager::load(){
    std::ifstream in(manifestPath);
    if(!in.is_open()){
        return;
    }
    sstableFilesPerLevel.clear();
    sstableFilesPerLevel.resize(4);
    std::string line;
    int currentLevel = -1;
    while(std::getline(in, line)){
        if(line.find("LEVEL:") == 0){
            currentLevel = std::stoi(line.substr(6));
            continue;
        }
        if(!line.empty() && currentLevel >= 0 && currentLevel < 4)
            sstableFilesPerLevel[currentLevel].push_back(line);
    }
}

void ManifestManager::save() const{
    std::ofstream out(manifestPath, std::ios::trunc);
    for(int level = 0; level < (int)sstableFilesPerLevel.size(); ++level){
        out << "LEVEL:" << level << "\n";
        for(const auto& file : sstableFilesPerLevel[level]){
            out << file << "\n";
        }
    }
}

void ManifestManager::addSSTable(int level, const std::string& filePath){
    if(level >= 0 && level < (int)sstableFilesPerLevel.size())
        sstableFilesPerLevel[level].push_back(filePath);
}

const std::vector<std::vector<std::string>>& ManifestManager::getAllSSTables() const{
    return sstableFilesPerLevel;
}

void ManifestManager::clear(){
    for(auto& v : sstableFilesPerLevel) v.clear();
}
