#include "MergeIterator.h"

MergeIterator::MergeIterator(std::vector<Iterator*> inputs){
    for(size_t i = 0; i < inputs.size(); i++){
        if (inputs[i] && inputs[i]->valid()) {
            heap.push({inputs[i], i});
        }
    }
    advance();
}


bool MergeIterator::valid() const{
    return isValid;
}

const std::string& MergeIterator::key() const{
    return curKey;
}

const std::string& MergeIterator::value() const{
    return curValue;
}


void MergeIterator::next(){
    advance();
}


void MergeIterator::advance(){
    if (heap.empty()) {
        isValid = false;
        return;
    }

    Node top = heap.top();
    heap.pop();

    curKey = top.it->key();
    curValue = top.it->value();
    isValid = true;

    // Skip duplicates with lower priority
    while(!heap.empty() && heap.top().it->key() == curKey){
        Node dup = heap.top();
        heap.pop();
        dup.it->next();
        if (dup.it->valid()) {
            heap.push(dup);
        }
    }

    // Advance the chosen iterator
    top.it->next();
    if(top.it->valid()){
        heap.push(top);
    }
}
