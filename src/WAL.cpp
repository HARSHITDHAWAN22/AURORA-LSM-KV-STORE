#include "WAL.h"
#include "MemTable.h"
#include "Logger.h"

#include <fstream>
#include <filesystem>
#include <cstring>
#include <mutex>

enum OpCode : uint8_t {
    PUT = 1,
    DEL = 2
};

WAL::WAL(const std::string& path, size_t batchSize)
    : path(path), batchSize(batchSize){

    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path()
    );

    LOG_INFO("WAL initialized at path: " + path);
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
        LOG_DEBUG("WAL batch threshold reached, flushing");
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
        LOG_DEBUG("WAL batch threshold reached, flushing");
        flush();
    }
}

void WAL::flush(){
    std::lock_guard<std::mutex> lock(mtx);

    if(buffer.empty())
        return;

    std::ofstream out(path,std::ios::binary | std::ios::app);

    if(!out.is_open()){
        LOG_ERROR("WAL flush failed: unable to open file");
        return;
    }

    out.write(buffer.data(), buffer.size());

    if(!out.good()){
        LOG_ERROR("WAL flush failed: write error");
        return;
    }

    out.flush();
    buffer.clear();

    LOG_DEBUG("WAL flushed to disk");
}

void WAL::replay(MemTable& memTable){

    std::ifstream in(path,std::ios::binary);

    if(!in.is_open()){
        LOG_DEBUG("WAL replay skipped: file not found");
        return;
    }

    LOG_INFO("WAL replay started");

    size_t replayedOps = 0;

    while(true){

        uint8_t op;
        uint32_t keyLen, valLen;

        if(!in.read(reinterpret_cast<char*>(&op),1))
            break;

        in.read(reinterpret_cast<char*>(&keyLen),4);
        in.read(reinterpret_cast<char*>(&valLen),4);

        if(!in.good()){
            LOG_ERROR("WAL replay aborted: corrupted header");
            break;
        }

        std::string key(keyLen,'\0');
        in.read(&key[0],keyLen);

        if(op == PUT){
            std::string value(valLen,'\0');
            in.read(&value[0],valLen);
            memTable.put(key,value);
        }
        else if(op == DEL){
            memTable.put(key, MemTable::TOMBSTONE);
        }

        replayedOps++;
    }

    LOG_INFO("WAL replay completed. Operations replayed: " + std::to_string(replayedOps));
}

void WAL::clear(){
    std::lock_guard<std::mutex> lock(mtx);

    std::ofstream out(path,std::ios::binary | std::ios::trunc);

    if(!out.is_open()){
        LOG_ERROR("WAL clear failed: unable to open file");
        return;
    }

    LOG_DEBUG("WAL cleared after flush");
}