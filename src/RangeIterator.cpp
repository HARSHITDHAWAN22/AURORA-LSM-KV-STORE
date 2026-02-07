#include "RangeIterator.h"

RangeIterator::RangeIterator(Iterator* it,
                             const std::string& start,
                             const std::string& end)
    : base(it), startKey(start), endKey(end){
    advanceToRange();

}

bool RangeIterator::valid() const{
    return isValid;
}


const std::string& RangeIterator::key() const{
    return base->key();
}

const std::string& RangeIterator::value() const{
    return base->value();
}


void RangeIterator::next(){
    base->next();
    advanceToRange();
}

void RangeIterator::advanceToRange(){
    while (base->valid()) {
        if (base->key() < startKey) {
            base->next();
        } else if (!endKey.empty() && base->key() >= endKey){
            isValid = false;
            return;
        } else {
            isValid = true;
            return;
        }
    }
    isValid = false;
}
