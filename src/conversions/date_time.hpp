#pragma once

#include <chrono>

namespace conversions {
    struct date_time {
        static std::chrono::system_clock::time_point from_string(const std::string &str) {
            // like 2024-11-17T17:42:11+00:00
            std::tm tm{};
            std::istringstream ss(str);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            if (ss.fail()) {
                throw std::runtime_error("Failed to parse date time string");
            }
            return std::chrono::system_clock::from_time_t(std::mktime(&tm));
        }
    };
}