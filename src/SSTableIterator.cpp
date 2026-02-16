#include "SSTableIterator.h"



SSTableIterator::SSTableIterator(const SSTable& t)
    : table(t),
      in(t.getFilePath(), std::ios::binary) {

    if (!in.is_open()) {
        isValid = false;
        return;
    }

    // Read footer
    in.seekg(-static_cast<std::streamoff>(sizeof(uint64_t) * 2), std::ios::end);

    uint64_t indexOffset;
    uint64_t magic;

    in.read(reinterpret_cast<char*>(&indexOffset), sizeof(indexOffset));
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));

    dataEnd = indexOffset;

    // Reset to beginning
    in.seekg(0);

    loadNext();
}


void SSTableIterator::loadNext(){

    if(!in.good() || static_cast<uint64_t>(in.tellg()) >= dataEnd){
        isValid = false;
        return;
    }

    uint32_t k, v;

    if(!in.read(reinterpret_cast<char*>(&k), sizeof(k))){
        isValid = false;
        return;
    }

    if(static_cast<uint64_t>(in.tellg()) + k > dataEnd){
        isValid = false;
        return;
    }

    curKey.resize(k);
    in.read(&curKey[0], k);

    if(!in.read(reinterpret_cast<char*>(&v), sizeof(v))){
        isValid = false;
        return;
    }

    if(static_cast<uint64_t>(in.tellg()) + v > dataEnd){
        isValid = false;
        return;
    }

    curValue.resize(v);
    in.read(&curValue[0], v);

    isValid = true;
}

bool SSTableIterator::valid() const {
    return isValid;
}

void SSTableIterator::next() {
    loadNext();
}

const std::string& SSTableIterator::key() const {
    return curKey;
}

const std::string& SSTableIterator::value() const {
    return curValue;
}
