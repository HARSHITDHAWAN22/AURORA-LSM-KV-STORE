#include "ManifestManager.h"
#include <fstream>
#include <filesystem>

ManifestManager::ManifestManager(const std::string& manifestPath)
    : manifestPath(manifestPath){}

void ManifestManager::load(){
    std::ifstream in(manifestPath);
    if(!in.is_open()){
        return;
    }

    std::string line;
    while(std::getline(in, line)){
        if(!line.empty())
            sstableFiles.push_back(line);
    }
}

void ManifestManager::save() const{
    // ensure directory exists
    std::filesystem::create_directories(
        std::filesystem::path(manifestPath).parent_path()
    );

    std::ofstream out(manifestPath);
    if(!out.is_open()){
        return;
    }

    for(const auto& file : sstableFiles){
        out << file << "\n";
    }
}

void ManifestManager::addSSTable(const std::string& filePath){
    sstableFiles.push_back(filePath);
}

const std::vector<std::string>& ManifestManager::getSSTables() const{
    return sstableFiles;
}
