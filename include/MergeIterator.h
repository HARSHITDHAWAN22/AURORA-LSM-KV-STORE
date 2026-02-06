#ifndef MERGE_ITERATOR_H
#define MERGE_ITERATOR_H


#include "Iterator.h"
#include <vector>
#include <queue>

class MergeIterator : public Iterator{

public:

    explicit MergeIterator(std::vector<Iterator*> inputs);

    bool valid() const override;
    void next() override;

    const std::string& key() const override;
    const std::string& value() const override;

private:

    struct Node{
        Iterator* it;
        size_t priority; // lower = higher priority (MemTable first)
    };


    struct Compare{
        bool operator()(const Node& a, const Node& b) const{

            if(a.it->key() != b.it->key())
                return a.it->key() > b.it->key(); // min-heap
            return a.priority > b.priority;
        }
    };

    
    void advance();

    std::priority_queue<Node, std::vector<Node>, Compare> heap;
    std::string curKey;
    std::string curValue;
    bool isValid = false;
};

#endif
