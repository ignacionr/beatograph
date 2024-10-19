#pragma once
#include <chrono>
#include <functional>
#include <string>
#include "metric_value.hpp"

struct metrics_parser {
    void operator()(std::string_view contents);
    void parse_line(std::string_view line);

    std::function<void(std::string_view,std::string_view)> metric_type;
    std::function<void(std::string_view,std::string_view)> metric_help;
    std::function<void(std::string_view,metric_value&&)> metric_metric_value;
    std::string buffer;
    std::chrono::system_clock::time_point sample_time;
};