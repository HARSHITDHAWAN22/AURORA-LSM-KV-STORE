#include "WAL.h"
#include "MemTable.h"
#include <cstdio>

#ifdef _WIN32
#include <io.h>
#define fsync _commit
#else
#include <unistd.h>
#endif

WAL::WAL(const std::string& path):walPath(path){}

void WAL::logPut(const std::string& key,const std::string& value){
    buffer.push_back("PUT "+key+" "+value);
}

void WAL::logDelete(const std::string& key){
    buffer.push_back("DEL "+key);
}

void WAL::flush(){
    if(buffer.empty()) return;

    FILE* fp = fopen(walPath.c_str(),"a");
    if(!fp) return;

    for(const auto& rec:buffer){
        fprintf(fp,"%s\n",rec.c_str());
    }

    fflush(fp);
    fsync(fileno(fp));   //durable on disk
    fclose(fp);

    buffer.clear();
}

void WAL::replay(MemTable& memTable){
    FILE* fp = fopen(walPath.c_str(),"r");
    if(!fp) return;

    char op[8],key[256],value[256];
    while(fscanf(fp,"%7s %255s",op,key)==2){
        if(strcmp(op,"PUT")==0){
            fscanf(fp,"%255s",value);
            memTable.put(key,value);
        }else if(strcmp(op,"DEL")==0){
            memTable.remove(key);
        }
    }

    fclose(fp);
}

void WAL::clear(){
    FILE* fp = fopen(walPath.c_str(),"w");
    if(fp) fclose(fp);
}