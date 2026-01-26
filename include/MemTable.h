#ifndef MEMTABLE_H
#define MEMTABLE_H

#include<string>
#include<map>
#include<mutex>

class MemTable{
public:
    explicit MemTable(int maxEntries);

    void put(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& value) const;
    void remove(const std::string& key);
    static const std::string TOMBSTONE;

    bool isFull() const;
     bool isEmpty() const;
    void clear();

    const std::map<std::string, std::string>& getData() const;

   


private:
    mutable std::mutex mtx;
    std::map<std::string, std::string> table;
    int maxEntries;
};

#endif
