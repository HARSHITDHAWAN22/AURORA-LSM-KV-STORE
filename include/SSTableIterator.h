#ifndef SSTABLE_ITERATOR_H
#define SSTABLE_ITERATOR_H

#include "Iterator.h"
#include "SSTable.h"
#include <fstream>

class SSTableIterator : public Iterator{
public:


    explicit SSTableIterator(const SSTable& table);

    bool valid() const override;
    void next() override;

    const std::string& key() const override;
    const std::string& value() const override;


private:
uint64_t dataEnd = 0;

    void loadNext();

    const SSTable& table;
    std::ifstream in;

    std::string curKey;
    std::string curValue;
    bool isValid = false;
    
};


#endif
