#ifndef MEMTABLE_H
#define MEMTABLE_H

#include<string>
#include<map>

class MemTable{
public:
    explicit MemTable(int maxEntries);

    void put(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& value) const;
    void remove(const std::string& key);

    bool isFull() const;
    void clear();

    const std::map<std::string, std::string>& getData() const;


private:

    std::map<std::string, std::string> table;
    int maxEntries;
};

#endif
