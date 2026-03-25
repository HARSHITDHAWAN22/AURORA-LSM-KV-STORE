#pragma once
#include <string>

enum class WorkloadType{
    WRITE_HEAVY,
    READ_HEAVY,
    MIXED
};

enum class KeyDistribution{
    SEQUENTIAL,
    RANDOM
};

struct BenchmarkConfig{
    int total_ops = 10000;

    WorkloadType workload = WorkloadType::MIXED;
    KeyDistribution key_dist = KeyDistribution::RANDOM;

    int key_space = 100000;   // range of keys
    int value_size = 100;     // bytes

    double put_ratio = 0.5;
    double get_ratio = 0.45;
    double delete_ratio = 0.05;
};