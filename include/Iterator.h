#ifndef ITERATOR_H
#define ITERATOR_H

#include <string>

class Iterator{
public:

    virtual ~Iterator() = default;

    virtual bool valid() const = 0;
    virtual void next() = 0;

    virtual const std::string& key() const = 0;
    virtual const std::string& value() const = 0;
};

#endif
