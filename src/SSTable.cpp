#include "SSTable.h"
#include <fstream>

SSTable::SSTable(const std::string& filePath)
    : filePath(filePath) {}

bool SSTable::writeToDisk(const std::map<std::string, std::string>& data){
    std::ofstream out(filePath);
    if(!out.is_open()){
        return false;
    }

    for(const auto& entry : data){

        out << entry.first << ":" << entry.second << "\n";
    }

    out.close();
    return true;
}

bool SSTable::get(const std::string& key, std::string& value) const{
    std::ifstream in(filePath);
    if (!in.is_open()) {
        return false;
    }

    std::string line;
    while(std::getline(in, line)){

        auto pos = line.find(':');
        if (pos == std::string::npos) continue;

        std::string currentKey = line.substr(0, pos);

        if(currentKey == key){
            value = line.substr(pos + 1);
            return true;
        }
    }

    return false;
}

const std::string& SSTable::getFilePath() const{
    return filePath;
}
