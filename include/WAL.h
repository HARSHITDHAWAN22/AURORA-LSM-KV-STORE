#ifndef WAL_H
#define WAL_H

#include <string>

class MemTable;

class WAL{
public:
    explicit WAL(const std::string& path);

    void logPut(const std::string& key,const std::string& value);
    void logDelete(const std::string& key);

    void replay(MemTable& memTable);
    void clear();

private:

    std::string walPath;
};

#endif