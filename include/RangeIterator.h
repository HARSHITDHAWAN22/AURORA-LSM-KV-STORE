#ifndef RANGE_ITERATOR_H
#define RANGE_ITERATOR_H

#include "Iterator.h"
#include <string>

class RangeIterator : public Iterator{
public:

    RangeIterator(Iterator* base,
                  const std::string& start,
                  const std::string& end);

    bool valid() const override;
    void next() override;

    const std::string& key() const override;
    const std::string& value() const override;

    
private:
    void advanceToRange();

    Iterator* base;
    std::string startKey;
    std::string endKey;

    bool isValid = false;
};

#endif
