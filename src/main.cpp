#include "KVStore.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>

#include "Logger.h"

// Load compaction strategy
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

        Logger::getInstance().init("aurora.log", LogLevel::INFO);

        if (argc < 2) {

            std::cout << "Usage:\n";
            std::cout << "  aurorakv shell\n";
            std::cout << "  aurorakv start <leveling|tiering>\n";

            return 0;
        }

        std::string command = argv[1];

        // ==========================
        // SHELL MODE
        // ==========================
        if (command == "shell") {

            std::string strategy = loadStrategy();

            KVStore store("config/system_config.json", strategy);

            std::cout << "AuroraKV Shell Mode\n";
            std::cout << "Type 'exit' to quit\n";

            std::string line;

            while (true) {

                try {

                    std::cout << ">> ";
                    std::getline(std::cin >> std::ws, line);

                    if (line.empty())
                        continue;

                    std::stringstream ss(line);

                    std::string cmd;
                    ss >> cmd;

                    if (cmd == "exit")
                        break;

                    // ------------------
                    // PUT
                    // ------------------
                    else if (cmd == "put") {

                        std::string k, v;
                        ss >> k >> v;

                        store.put(k, v);
                        std::cout << "OK\n";
                    }

                    // ------------------
                    // GET
                    // ------------------
                    else if (cmd == "get") {

                        std::string k, val;
                        ss >> k;

                        if (store.get(k, val))
                            std::cout << val << "\n";
                        else
                            std::cout << "NOT FOUND\n";
                    }

                    // ------------------
                    // DELETE
                    // ------------------
                    else if (cmd == "delete") {

                        std::string k;
                        ss >> k;

                        store.deleteKey(k);
                        std::cout << "DELETED\n";
                    }

                    // ------------------
                    // FLUSH
                    // ------------------
                    else if (cmd == "flush") {

                        store.flush();
                        std::cout << "FLUSHED\n";
                    }

                    // ------------------
                    // SCAN  ✅ FIX ADDED
                    // ------------------
                    else if (cmd == "scan") {

                        std::string start, end;
                        ss >> start >> end;

                        if (start.empty() || end.empty()) {
                            std::cout << "Usage: scan <start> <end>\n";
                            continue;
                        }

                        store.scan(start, end);
                    }

                    // ------------------
                    // STATS
                    // ------------------
                    else if (cmd == "stats") {

                        store.printStats();
                    }

                    // ------------------
                    // BENCHMARK
                    // ------------------
                    else if (cmd == "bench") {

                        int n;
                        ss >> n;

                        if (n <= 0) {
                            std::cout << "Usage: bench <operations>\n";
                            continue;
                        }

                        auto start = std::chrono::steady_clock::now();

                        for (int i = 0; i < n; i++) {
                            store.put("k" + std::to_string(i),
                                      "v" + std::to_string(i));
                        }

                        for (int i = 0; i < n; i++) {

                            std::string val;
                            store.get("k" + std::to_string(i), val);
                        }

                        auto end = std::chrono::steady_clock::now();

                        double seconds =
                            std::chrono::duration<double>(end - start).count();

                        std::cout << "Benchmark completed\n";
                        std::cout << "Operations: " << (n * 2) << "\n";
                        std::cout << "Time: " << seconds << " sec\n";
                        std::cout << "Throughput: "
                                  << (n * 2) / seconds
                                  << " ops/sec\n";
                    }

                    else {
                        std::cout << "Unknown command\n";
                    }

                } catch (const std::exception& e) {

                    std::cout << "Error: " << e.what() << "\n";
                }
            }

            return 0;
        }

        // ==========================
        // SET STRATEGY
        // ==========================
        else if (command == "start") {

            if (argc < 3) {
                std::cout << "Specify compaction strategy: leveling | tiering\n";
                return 0;
            }

            std::string strategy = argv[2];

            if (strategy != "leveling" && strategy != "tiering") {
                std::cout << "Invalid strategy\n";
                return 0;
            }

            saveStrategy(strategy);
            std::cout << "Strategy set to " << strategy << "\n";

            return 0;
        }

        std::cout << "Unknown command\n";
        return 0;

    } catch (...) {

        std::cerr << "Fatal error\n";
        return 1;
    }
}