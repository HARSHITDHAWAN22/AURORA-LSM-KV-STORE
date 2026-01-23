#include "SSTable.h"
#include <fstream>

SSTable::SSTable(const std::string& filePath,
                 size_t bloomBitSize,
                 size_t bloomHashCount)
    : filePath(filePath),
      bloom(bloomBitSize, bloomHashCount){
    loadBloom();
}

void SSTable::loadBloom(){
    std::ifstream in(filePath);
    if(!in.is_open()) return;

    std::string line;
    while(std::getline(in, line)){
        auto pos = line.find('\t');
        if(pos == std::string::npos) continue;
        bloom.add(line.substr(0, pos));
    }
}

bool SSTable::writeToDisk(const std::map<std::string, std::string>& data){
    std::ofstream out(filePath);
    if(!out.is_open()) return false;

    for(const auto& entry : data){
        out << entry.first << "\t" << entry.second << "\n";
        bloom.add(entry.first);
    }
    return true;
}

bool SSTable::get(const std::string& key, std::string& value) const{
    if(!bloom.mightContain(key)) return false;

    std::ifstream in(filePath);
    if(!in.is_open()) return false;

    std::string line;
    while(std::getline(in, line)){
        auto pos = line.find('\t');
        if(pos == std::string::npos) continue;

        if(line.substr(0, pos) == key){
            value = line.substr(pos + 1);
            return true;
        }
    }
    return false;
}

const std::string& SSTable::getFilePath() const{
    return filePath;
}
