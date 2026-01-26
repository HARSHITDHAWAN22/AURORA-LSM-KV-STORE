#include "WAL.h"
#include "MemTable.h"

#include <fstream>
#include <filesystem>
#include <cstring>
#include<mutex>
std::mutex mtx;


enum OpCode : uint8_t {
    PUT = 1,
    DEL = 2
};

WAL::WAL(const std::string& path,size_t batchSize)
    : path(path), batchSize(batchSize){

    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path()
    );
}

void WAL::appendUInt32(uint32_t v){
    for(int i = 0; i < 4; ++i){
        buffer.push_back(static_cast<char>((v >> (i * 8)) & 0xFF));
    }
}

void WAL::logPut(const std::string& key,const std::string& value){
    std::lock_guard<std::mutex> lock(mtx);
    buffer.push_back(static_cast<char>(PUT));
    appendUInt32(static_cast<uint32_t>(key.size()));
    appendUInt32(static_cast<uint32_t>(value.size()));

    buffer.insert(buffer.end(), key.begin(), key.end());
    buffer.insert(buffer.end(), value.begin(), value.end());

    if(buffer.size() >= batchSize * 64){
        flush();
    }
}

void WAL::logDelete(const std::string& key){
    std::lock_guard<std::mutex> lock(mtx);
    buffer.push_back(static_cast<char>(DEL));
    appendUInt32(static_cast<uint32_t>(key.size()));
    appendUInt32(0);

    buffer.insert(buffer.end(), key.begin(), key.end());

    if(buffer.size() >= batchSize * 64){
        flush();
    }
}

void WAL::flush(){
    std::lock_guard<std::mutex> lock(mtx);
    if(buffer.empty()) return;

    std::ofstream out(path,std::ios::binary | std::ios::app);
    out.write(buffer.data(), buffer.size());
    out.flush();

    buffer.clear();
}

void WAL::replay(MemTable& memTable){
    std::ifstream in(path,std::ios::binary);
    if(!in.is_open()) return;

    while(true){
        uint8_t op;
        uint32_t keyLen, valLen;

        if(!in.read(reinterpret_cast<char*>(&op),1)) break;
        in.read(reinterpret_cast<char*>(&keyLen),4);
        in.read(reinterpret_cast<char*>(&valLen),4);

        std::string key(keyLen,'\0');
        in.read(&key[0],keyLen);

        if(op == PUT){
            std::string value(valLen,'\0');
            in.read(&value[0],valLen);
            memTable.put(key,value);
        }
        else if(op == DEL){
            memTable.remove(key);
        }
    }
}

void WAL::clear(){
    std::lock_guard<std::mutex> lock(mtx);
    std::ofstream out(path,std::ios::binary | std::ios::trunc);
}