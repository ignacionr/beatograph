#pragma once
#include <unordered_map>
#include "metric.hpp"

struct metric_value {
    std::unordered_map<std::string, std::string> labels;
    double value;
};
