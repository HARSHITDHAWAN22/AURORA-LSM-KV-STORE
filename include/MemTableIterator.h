#ifndef MEMTABLE_ITERATOR_H
#define MEMTABLE_ITERATOR_H

#include "Iterator.h"
#include <map>

class MemTableIterator : public Iterator{

public:

    using MapIter = std::map<std::string, std::string>::const_iterator;

    MemTableIterator(MapIter begin, MapIter end)
        : it(begin), endIt(end) {}

    bool valid() const override{
        return it != endIt;
    }

    void next() override{
        if (it != endIt) ++it;
    }

    const std::string& key() const override{
        return it->first;
    }

    const std::string& value() const override {
        return it->second;
    }

    
private:
    MapIter it;
    MapIter endIt;
};

#endif
