#include "ManifestManager.h"
#include <fstream>
#include <filesystem>

ManifestManager::ManifestManager(const std::string& manifestPath)
    : manifestPath(manifestPath){
    sstableFilesPerLevel.clear();
    sstableFilesPerLevel.resize(4); // MAX_LEVELS

    levelBytes.clear();
    levelBytes.resize(4, 0ULL);
}

void ManifestManager::load(){
    std::ifstream in(manifestPath);
    if(!in.is_open()){
        return;
    }

    sstableFilesPerLevel.clear();
    sstableFilesPerLevel.resize(4);

    levelBytes.clear();
    levelBytes.resize(4, 0ULL);

    std::string line;
    int currentLevel = -1;

    while(std::getline(in, line)){
        if(line.find("LEVEL:") == 0){
            currentLevel = std::stoi(line.substr(6));
            continue;
        }

        if(!line.empty() && currentLevel >= 0 && currentLevel < 4){
            sstableFilesPerLevel[currentLevel].push_back(line);

            if(std::filesystem::exists(line)){
                levelBytes[currentLevel] +=
                    std::filesystem::file_size(line);
            }
        }
    }
}



void ManifestManager::save() const{

    std::string tempPath = manifestPath + ".tmp";

    std::ofstream out(tempPath, std::ios::trunc);
    if(!out.is_open())
        return;

    for(int level = 0; level < (int)sstableFilesPerLevel.size(); ++level){
        out << "LEVEL:" << level << "\n";
        for(const auto& file : sstableFilesPerLevel[level]){
            out << file << "\n";
        }
    }

    out.close();

    std::filesystem::rename(tempPath, manifestPath);
}

std::size_t ManifestManager::levelFileCount(int level) const{
    if(level < 0 || level >= (int)sstableFilesPerLevel.size())
        return 0;

    return sstableFilesPerLevel[level].size();
}


void ManifestManager::addSSTable(int level, const std::string& filePath,std::uint64_t fileSize){
    if(level >= 0 && level < (int)sstableFilesPerLevel.size()){
        sstableFilesPerLevel[level].push_back(filePath);
        levelBytes[level] += fileSize;}
}


void ManifestManager::removeSSTable(const std::string& filePath){
    for(int level = 0; level < (int)sstableFilesPerLevel.size(); ++level){
        auto& files = sstableFilesPerLevel[level];
        for(auto it = files.begin(); it != files.end(); ++it){
            if(*it == filePath){
                if(std::filesystem::exists(filePath)){
                    levelBytes[level] -=
                        std::filesystem::file_size(filePath);
                }
                files.erase(it);
                return;
            }
        }
    }
}

std::uint64_t ManifestManager::levelMaxBytes(int level) const{
    const std::uint64_t base = 10ULL * 1024ULL * 1024ULL; // 10MB

    if(level == 0)
        return base;

    std::uint64_t size = base;
    for(int i = 0; i < level; ++i)
        size *= 10ULL;

    return size;
}


bool ManifestManager::levelOverflow(int level) const{
    if(level < 0 || level >= (int)sstableFilesPerLevel.size())
        return false;

    if(level == 0){
        return levelFileCount(0) > 4;
    }

    return levelBytes[level] > levelMaxBytes(level);
}


const std::vector<std::vector<std::string>>& ManifestManager::getAllSSTables() const{
    return sstableFilesPerLevel;
}


void ManifestManager::clear(){
    for(auto& v : sstableFilesPerLevel) v.clear();
    std::fill(levelBytes.begin(),
          levelBytes.end(),
          0ULL);
}
