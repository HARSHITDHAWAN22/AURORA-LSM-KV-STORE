#include "KVStore.h"
#include <iostream>
#include <fstream>
#include <string>

static std::string loadStrategy(){
    std::ifstream in("metadata/strategy.txt");
    std::string s;
    if(in>>s && (s=="leveling" || s=="tiering"))
        return s;
    return "leveling";
}

static void saveStrategy(const std::string& s){
    std::ofstream out("metadata/strategy.txt",std::ios::trunc);
    out<<s;
}

int main(int argc,char* argv[]){
    if(argc<2){
        std::cout<<"Usage:\n";
        std::cout<<"  aurorakv start <leveling|tiering>\n";
        std::cout<<"  aurorakv put <key> <value>\n";
        std::cout<<"  aurorakv get <key>\n";
        std::cout<<"  aurorakv delete <key>\n";
        std::cout<<"  aurorakv scan <start> <end>\n";
        return 0;
    }

    std::string command=argv[1];

    // -------- START (set strategy once) --------
    if(command=="start"){
        if(argc!=3){
            std::cerr<<"start requires <leveling|tiering>\n";
            return 1;
        }

        std::string strategy=argv[2];
        if(strategy!="leveling" && strategy!="tiering"){
            std::cerr<<"Invalid strategy\n";
            return 1;
        }

        saveStrategy(strategy);
        std::cout<<"Strategy set to "<<strategy<<"\n";
        return 0;
    }

    // -------- ALL OTHER COMMANDS --------
    std::string strategy=loadStrategy();
    KVStore store("config/system_config.json",strategy);

    if(command=="put"){
        if(argc!=4){
            std::cerr<<"put requires <key> <value>\n";
            return 1;
        }
        store.put(argv[2],argv[3]);
        std::cout<<"OK\n";
    }
    else if(command=="get"){
        if(argc!=3){
            std::cerr<<"get requires <key>\n";
            return 1;
        }
        std::string value;
        if(store.get(argv[2],value))
            std::cout<<value<<"\n";
        else
            std::cout<<"NOT FOUND\n";
    }
    else if(command=="delete"){
        if(argc!=3){
            std::cerr<<"delete requires <key>\n";
            return 1;
        }
        store.deleteKey(argv[2]);
        std::cout<<"DELETED\n";
    }
    else if(command=="scan"){
        if(argc!=4){
            std::cerr<<"scan requires <start> <end>\n";
            return 1;
        }
        store.scan(argv[2],argv[3]);
    }
    else{
        std::cerr<<"Unknown command\n";
    }

    // -------- FLUSH ON EXIT (CRITICAL) --------
    store.flush();

    return 0;
}