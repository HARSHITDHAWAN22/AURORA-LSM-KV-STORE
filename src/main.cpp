#include "KVStore.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "Logger.h"

// Load compaction strategy from file
static std::string loadStrategy() {
    std::ifstream in("metadata/strategy.txt");
    std::string s;
    if (in >> s && (s == "leveling" || s == "tiering"))
        return s;
    return "leveling";
}

// Save compaction strategy
static void saveStrategy(const std::string& s) {
    std::ofstream out("metadata/strategy.txt", std::ios::trunc);
    out << s;
}

int main(int argc, char* argv[]) {
    try {

        // Initialize thread-safe logging
        Logger::getInstance().init("aurora.log", LogLevel::INFO);

        if (argc < 2) {
            std::cout << "Usage:\n";
            std::cout << "  aurorakv shell\n";
            std::cout << "  aurorakv start <leveling|tiering>\n";
            return 0;
        }

        std::string command = argv[1];

        // ===========================
        // SHELL MODE
        // ===========================
        if (command == "shell") {

            std::string strategy = loadStrategy();
            Logger::getInstance().info("Starting AuroraKV shell with strategy: " + strategy);

            KVStore store("config/system_config.json", strategy);

            std::cout<< "AuroraKV Shell Mode\n";
            std::cout<< "Type 'exit' to quit\n";

           std::string line;

while(true){

    try{

        std::cout << ">> ";
        std::getline(std::cin >> std::ws, line);

        if(line.empty()) continue;

        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if(cmd == "exit") break;

        else if(cmd == "put"){
            std::string k,v;
            ss >> k >> v;

            store.put(k,v);
            Logger::getInstance().debug("PUT key=" + k);

            std::cout << "OK\n";
        }

        else if(cmd == "get"){
            std::string k,val;
            ss >> k;

            if(store.get(k,val)){
                std::cout << val << "\n";
            }else{
                std::cout << "NOT FOUND\n";
            }
        }

        else if(cmd == "delete"){
            std::string k;
            ss >> k;

            store.deleteKey(k);
            std::cout << "DELETED\n";
        }

        else if(cmd == "flush"){
            store.flush();
            std::cout << "FLUSHED\n";
        }

        else if(cmd == "stats"){
            store.printStats();
        }

        else{
            std::cout << "Unknown command\n";
        }

    }
    catch(const std::exception& e){
        Logger::getInstance().error("Shell error: " + std::string(e.what()));
        std::cout << "Error: " << e.what() << "\n";
    }
}

            Logger::getInstance().info("Shell exited normally");
            return 0;
        }

        // ===========================
        // SET STRATEGY
        // ===========================
        else if (command == "start") {

            if(argc < 3){
                std::cout << "Specify compaction strategy: leveling | tiering\n";
                return 0;
            }

            std::string strategy = argv[2];

            if(strategy != "leveling" && strategy != "tiering"){
                std::cout << "Invalid strategy. Use leveling or tiering.\n";
                return 0;
            }

            saveStrategy(strategy);
            Logger::getInstance().info("Compaction strategy set to " + strategy);

            std::cout << "Strategy set to " << strategy << "\n";
            return 0;
        }

        std::cout << "Unknown command\n";
        return 0;

    } catch(const std::exception& e){

        Logger::getInstance().error("Fatal error: " + std::string(e.what()));
        std::cerr << "Fatal error occurred. Check aurora.log\n";
        return 1;
    }
}