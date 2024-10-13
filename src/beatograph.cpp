#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include "metric.hpp"
#include "metric_value.hpp"
#include "metrics_parser.hpp"
#include "metrics_model.hpp"

int main() {
    std::ifstream file("sample/metrics.txt");
    std::string line;
    metrics_parser parser;
    metrics_model model;
    parser.metric_help = [&](const std::string_view& name, const std::string_view& help) {
        model.set_help(name, help);
    };
    parser.metric_type = [&](const std::string_view& name, const std::string_view& type) {
        model.set_type(name, type);
    };
    parser.metric_metric_value = [&](std::string_view name, const metric_value& value) {
        model.add_value(name, value);
    };
    while (std::getline(file, line)) {
        parser(line);
    }
    std::cout << "Loaded " << model.metrics.size() << " metrics" << std::endl;
    int total_values = std::accumulate(model.metrics.begin(), model.metrics.end(), 0,
        [](int acc, const auto& pair) {
            return acc + pair.second.size();
        });
    std::cout << "Loaded " << total_values << " values" << std::endl;

    std::cout << "The first metric is " << model.metrics.begin()->first.name << std::endl;
    std::cout << "  with help " << model.metrics.begin()->first.help << std::endl;
    std::cout << "  and type " << model.metrics.begin()->first.type << std::endl;
    std::cout << "The first value is " << model.metrics.begin()->second.front().value << std::endl;
    std::cout << "  with labels:" << std::endl;
    for (const auto& pair : model.metrics.begin()->second.front().labels) {
        std::cout << "    " << pair.first << " = " << pair.second << std::endl;
    }
    return 0;
}
