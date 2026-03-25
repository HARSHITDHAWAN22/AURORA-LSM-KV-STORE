#include <iostream>
#include <chrono>
#include "Workload.h"
#include "Config.h"
#include "../include/KVStore.h"

int main() {
    BenchmarkConfig config;

    // =========================
    // BENCHMARK CONFIG
    // =========================
    config.total_ops = 10000;
    config.workload = WorkloadType::MIXED;
    config.key_dist = KeyDistribution::RANDOM;

    config.put_ratio = 0.5;
    config.get_ratio = 0.45;
    config.delete_ratio = 0.05;

    // =========================
    // GENERATE WORKLOAD
    // =========================
    WorkloadGenerator generator(config);
    auto ops = generator.generate();

    // =========================
    // INIT AURORAKV
    // =========================
    KVStore db("config/system_config.json", "leveling");
    // strategy options:
    // "leveling"
    // "tiering"

    // =========================
    // RUN BENCHMARK
    // =========================
    auto start = std::chrono::high_resolution_clock::now();

    for (auto& op : ops) {
        if (op.type == OperationType::PUT) {
            db.put(op.key, op.value);
        }
        else if (op.type == OperationType::GET) {
            std::string value;
            db.get(op.key, value);
        }
        else {
            db.deleteKey(op.key);
        }
    }

    // Force flush remaining MemTable to disk
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

    db.printStats();

    return 0;
}