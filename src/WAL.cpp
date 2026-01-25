#include "WAL.h"
#include "MemTable.h"

#include <fstream>
#include <filesystem>
#include <sstream>

WAL::WAL(const std::string& path, size_t batchSize)
    : path(path), batchSize(batchSize) {

    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path()
    );
}

void WAL::logPut(const std::string& key,const std::string& value){
    buffer.push_back("PUT " + key + " " + value);

    if(buffer.size() >= batchSize){
        flush();
    }
}

void WAL::logDelete(const std::string& key){
    buffer.push_back("DEL " + key);

    if(buffer.size() >= batchSize){
        flush();
    }
}

void WAL::flush(){
    if(buffer.empty())
        return;

    std::ofstream out(path,std::ios::app);
    if(!out.is_open())
        return;

    for(const auto& entry : buffer){
        out << entry << "\n";
    }

    out.flush();
    buffer.clear();
}

void WAL::replay(MemTable& memTable){
    std::ifstream in(path);
    if(!in.is_open())
        return;

    std::string line;
    while(std::getline(in,line)){
        std::istringstream iss(line);
        std::string op,key,value;

        iss >> op >> key;

        if(op=="PUT"){
            iss >> value;
            memTable.put(key,value);
        }
        else if(op=="DEL"){
            memTable.remove(key);
        }
    }
}

void WAL::clear(){
    std::ofstream out(path,std::ios::trunc);
}