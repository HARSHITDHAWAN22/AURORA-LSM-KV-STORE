#include <iostream>
#include <chrono>
#include <filesystem>
#include "Workload.h"
#include "Config.h"
#include "../include/KVStore.h"

void cleanBenchmarkState() {
    namespace fs = std::filesystem;

    std::cout << "[0] Cleaning previous benchmark state...\n";

    // Remove metadata files
    fs::remove("metadata/wal.log");
    fs::remove("metadata/manifest.txt");
    fs::remove("metadata/stats.dat");

    // Remove old SSTables from correct directory
    if (fs::exists("data/sstables")) {
        for (const auto& entry : fs::directory_iterator("data/sstables")) {
            if (entry.path().extension() == ".dat") {
                fs::remove(entry.path());
            }
        }
    }

    std::cout << "[0] Clean benchmark state ready.\n";
}

int main() {
    cleanBenchmarkState();

    std::cout << "[1] Benchmark started...\n";

    BenchmarkConfig config;

    config.total_ops = 100000;
    config.workload = WorkloadType::READ_HEAVY;
    config.key_dist = KeyDistribution::RANDOM;

    config.put_ratio = 0.8;
    config.get_ratio = 0.15;
    config.delete_ratio = 0.05;

    std::cout << "[2] Generating workload...\n";
    WorkloadGenerator generator(config);
    auto ops = generator.generate();
    std::cout << "[3] Workload generated: " << ops.size() << " ops\n";

    std::cout << "[4] Initializing KVStore...\n";
    KVStore db("config/system_config.json", "tiering");
    std::cout << "[5] KVStore initialized successfully.\n";

    std::cout << "[6] Running benchmark...\n";
    auto start = std::chrono::high_resolution_clock::now();

    int count = 0;
    for (auto& op : ops) {
        count++;

      //  std::cout << "Running op #" << count << " ... ";

        if (op.type == OperationType::PUT) {
           // std::cout << "PUT(" << op.key << ")\n";
            db.put(op.key, op.value);
        }
        else if (op.type == OperationType::GET) {
          //  std::cout << "GET(" << op.key << ")\n";
            std::string value;
            db.get(op.key, value);
        }
        else {
          //  std::cout << "DELETE(" << op.key << ")\n";
            db.deleteKey(op.key);
        }

       // std::cout << "Done op #" << count << "\n";
    }

    std::cout << "[7] Flushing DB...\n";
    db.flush();

    auto end = std::chrono::high_resolution_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();

    std::cout << "\n==============================\n";
    std::cout << "AuroraKV Benchmark Result\n";
    std::cout << "==============================\n";
    std::cout << "Total Ops   : " << config.total_ops << "\n";
    std::cout << "Time        : " << seconds << " sec\n";
    std::cout << "Throughput  : " << (config.total_ops / seconds) << " ops/sec\n";
    std::cout << "==============================\n\n";

    std::cout << "[8] Printing DB stats...\n";
    db.printStats();

    std::cout << "[9] Benchmark completed successfully.\n";

    return 0;
}