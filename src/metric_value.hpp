#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include "metric.hpp"

struct metric_value {
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> labels;
    double value;
};
