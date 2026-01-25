#ifndef WAL_H
#define WAL_H

#include <string>
#include <vector>

class MemTable;

class WAL{
public:
    explicit WAL(const std::string& path,size_t batchSize=10);

    void logPut(const std::string& key,const std::string& value);
    void logDelete(const std::string& key);

    void replay(MemTable& memTable);

    void flush();   // write buffer to disk + fsync
    void clear();   // truncate WAL (after SSTable flush)

private:
    std::string path;
    std::vector<std::string> buffer;
    size_t batchSize;
};

#endif