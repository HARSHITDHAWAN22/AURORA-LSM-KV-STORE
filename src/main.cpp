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
[[maybe_unused]]static void saveStrategy(const std::string& s){
    std::ofstream out("metadata/strategy.txt", std::ios::trunc);
    out << s;
}

int main(int argc, char* argv[]) {

    if(argc < 2){
        std::cout << "Usage:\n";
        std::cout << "  aurorakv shell\n";
        std::cout << "  aurorakv start <leveling|tiering>\n";
        return 0;
    }

    std::string command = argv[1];

    if(command == "shell") {

        std::string strategy = loadStrategy();
        KVStore store("config/system_config.json", strategy);

        std::cout << "AuroraKV Shell Mode\n";
        std::cout << "Type 'exit' to quit\n";

        std::string cmd;

        while(true) {

            std::cout << ">> ";
            std::cin >> cmd;

            if(cmd == "exit") break;

            if(cmd == "put") {
                std::string k, v;
                std::cin >> k >> v;
                store.put(k, v);
                std::cout << "OK\n";
            }

            else if(cmd == "get") {
                std::string k, val;
                std::cin >> k;
                if(store.get(k, val))
                    std::cout << val << "\n";
                else
                    std::cout << "NOT FOUND\n";
            }

            else if(cmd == "delete") {
                std::string k;
                std::cin >> k;
                store.deleteKey(k);
                std::cout << "DELETED\n";
            }

            else if(cmd == "flush") {
                store.flush();
                std::cout << "FLUSHED\n";
            }

            else if(cmd == "stats") {
                store.printStats();
            }

            else {
                std::cout << "Unknown command\n";
            }
        }

        return 0;
    }

    std::cout << "Unknown command\n";
    return 0;
}
