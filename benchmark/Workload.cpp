#include "Workload.h"
#include <random>
#include <sstream>

std::mt19937 rng(std::random_device{}());

WorkloadGenerator::WorkloadGenerator(const BenchmarkConfig& config)
    : config(config) {}

std::string WorkloadGenerator::generateKey(int i) {
    if (config.key_dist == KeyDistribution::SEQUENTIAL) {
        return "key_" + std::to_string(i);
    } else {
        return generateRandomKey();
    }
}

std::string WorkloadGenerator::generateRandomKey() {
    std::uniform_int_distribution<int> dist(1, config.key_space);
    return "key_" + std::to_string(dist(rng));
}

std::string WorkloadGenerator::generateValue() {
    return std::string(config.value_size, 'x');
}

OperationType WorkloadGenerator::chooseOperation() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double r = dist(rng);

    if (r < config.put_ratio) return OperationType::PUT;
    else if (r < config.put_ratio + config.get_ratio) return OperationType::GET;
    else return OperationType::DELETE_OP;
}

std::vector<Operation> WorkloadGenerator::generate() {
    std::vector<Operation> ops;

    for (int i = 0; i < config.total_ops; i++) {
        Operation op;
        op.type = chooseOperation();

        if (op.type == OperationType::PUT) {
            op.key = generateKey(i);
            op.value = generateValue();
            existing_keys.push_back(op.key);
        }
        else if (op.type == OperationType::GET) {
            if (!existing_keys.empty()) {
                op.key = existing_keys[rng() % existing_keys.size()];
            } else {
                op.key = generateRandomKey();
            }
        }
        else { // DELETE
            if (!existing_keys.empty()) {
                int idx = rng() % existing_keys.size();
                op.key = existing_keys[idx];
                existing_keys.erase(existing_keys.begin() + idx);
            } else {
                op.key = generateRandomKey();
            }
        }

        ops.push_back(op);
    }

    return ops;
}