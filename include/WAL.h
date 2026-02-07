#ifndef WAL_H
#define WAL_H

#include <string>
#include <vector>
#include <cstdint>
#include<mutex>
class MemTable;

class WAL{
public:
    explicit WAL(const std::string& path,size_t batchSize = 10);

    void logPut(const std::string& key,const std::string& value);
    void logDelete(const std::string& key);

    void replay(MemTable& memTable);

    void flush();
    void clear();

private:
    std::string path;
    std::vector<char> buffer;   // binary buffer
    size_t batchSize;
std::mutex mtx;

    void appendUInt32(uint32_t v);
};

#endif