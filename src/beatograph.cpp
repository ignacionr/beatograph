#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string>
#include <unordered_map>
#include "metric.hpp"
#include "metric_value.hpp"
#include "metrics_parser.hpp"
#include "metrics_model.hpp"
#include "main_screen.hpp"

void print_metric(const auto& it) {
    std::cout << "Metric: " << it->first.name << std::endl;
    std::cout << "Help: " << it->first.help << std::endl;
    std::cout << "Type: " << it->first.type << std::endl;
    for (const auto& value : it->second) {
        std::cout << "Value: " << value.value << std::endl;
        for (const auto& label : value.labels) {
            std::cout << "Label: " << label.first << " = " << label.second << std::endl;
        }
    }
}

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

    main_screen screen{model};
    screen.run();

    return 0;
}
