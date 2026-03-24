#pragma once
#include <string>
#include <vector>
#include "Config.h"

enum class OperationType {
    PUT,
    GET,
    DELETE_OP
};

struct Operation {
    OperationType type;
    std::string key;
    std::string value;
};

class WorkloadGenerator {
public:
    WorkloadGenerator(const BenchmarkConfig& config);

    std::vector<Operation> generate();

private:
    BenchmarkConfig config;

    std::string generateKey(int i);
    std::string generateRandomKey();
    std::string generateValue();
    OperationType chooseOperation();

    std::vector<std::string> existing_keys;
};