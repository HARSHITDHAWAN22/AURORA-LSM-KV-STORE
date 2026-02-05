#include "SSTableIterator.h"



SSTableIterator::SSTableIterator(const SSTable& t)
    : table(t),
      in(t.getFilePath(), std::ios::binary) {
    loadNext();
}


bool SSTableIterator::valid() const{
    return isValid;
}

const std::string& SSTableIterator::key() const{
    return curKey;
}


const std::string& SSTableIterator::value() const{
    return curValue;
}


void SSTableIterator::next(){
    loadNext();
}

void SSTableIterator::loadNext(){
    if (!in.good()) {
        isValid = false;
        return;
    }

    uint32_t k, v;
    if(!in.read(reinterpret_cast<char*>(&k), sizeof(k))){
        isValid = false;
        return;
    }

    
    curKey.resize(k);
    in.read(&curKey[0], k);

    in.read(reinterpret_cast<char*>(&v), sizeof(v));
    curValue.resize(v);
    in.read(&curValue[0], v);

    isValid = true;
}
