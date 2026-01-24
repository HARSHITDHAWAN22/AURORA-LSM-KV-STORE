#include "WAL.h"
#include "MemTable.h"
#include <fstream>

WAL::WAL(const std::string& path):walPath(path){}

void WAL::logPut(const std::string& key,const std::string& value){
    std::ofstream out(walPath,std::ios::app);
    out<<"PUT "<<key<<" "<<value<<"\n";
}

void WAL::logDelete(const std::string& key){
    std::ofstream out(walPath,std::ios::app);
    out<<"DEL "<<key<<"\n";
}

void WAL::replay(MemTable& memTable){
    std::ifstream in(walPath);
    if(!in.is_open()) return;

    std::string op,key,value;
    while(in>>op>>key){
        if(op=="PUT"){
            in>>value;
            memTable.put(key,value);
        }
        else if(op=="DEL"){
            memTable.remove(key);
        }
    }
}

void WAL::clear(){
    std::ofstream out(walPath,std::ios::trunc);
}