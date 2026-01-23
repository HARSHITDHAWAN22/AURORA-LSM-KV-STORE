#include <iostream>
#include <string>
#include <algorithm>
#include "KVStore.h"

static std::string normalize(std::string s){
    s.erase(std::remove_if(s.begin(), s.end(),
            [](unsigned char c){ return c == '\r' || c == '\n'; }),
            s.end());
    return s;
}

int main(int argc, char* argv[]){
    if(argc < 2){
        std::cout
            << "Usage:\n"
            << "  aurorakv put <key> <value>\n"
            << "  aurorakv get <key>\n"
            << "  aurorakv delete <key>\n"
            << "  aurorakv scan <start_key> <end_key>\n";
        return 0;
    }

    KVStore store("config/system_config.json");
    std::string command = normalize(argv[1]);

    if(command == "put"){
        if(argc != 4){
            std::cerr << "put requires <key> <value>\n";
            return 1;
        }
        store.put(argv[2], argv[3]);
        store.flush();
        std::cout << "OK\n";
    }
    else if(command == "get"){
        if(argc != 3){
            std::cerr << "get requires <key>\n";
            return 1;
        }
        std::string value;
        if(store.get(argv[2], value))
            std::cout << value << "\n";
        else
            std::cout << "NOT FOUND\n";
    }
    else if(command == "delete"){
        if(argc != 3){
            std::cerr << "delete requires <key>\n";
            return 1;
        }
        store.deleteKey(argv[2]);
        store.flush();
        std::cout << "DELETED\n";
    }
    
    else if(command == "scan"){
        if(argc != 4){
            std::cerr << "scan requires <start_key> <end_key>\n";
            return 1;
        }
        store.scan(argv[2], argv[3]);
    }

    else{
        std::cerr << "Unknown command\n";
    }

    return 0;
}
