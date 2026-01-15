#ifndef SSTABLE_H
#define SSTABLE_H

#include<string>
#include<map>

class SSTable{
public:

    explicit SSTable(const std::string& filePath);

    // Write sorted data to disk
    bool writeToDisk(const std::map<std::string, std::string>& data);

    // Read a key from disk
    bool get(const std::string& key, std::string& value) const;

    const std::string& getFilePath() const;

private:

    std::string filePath;
};

#endif
