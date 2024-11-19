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
            // obtain the timezone, if there is one
            std::string timezone;
            ss >> timezone;
            auto ret = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            if (!timezone.empty() && timezone != "Z") {
                // adjust for timezone
                auto offset = std::stoi(timezone.substr(0, 3));
                ret += std::chrono::hours(offset);
            }
            return ret;
        }

        static std::chrono::system_clock::time_point from_dateonly_string(const std::string &str) {
            // like 2024-11-17
            std::tm tm{};
            std::istringstream ss(str);
            ss >> std::get_time(&tm, "%Y-%m-%d");
            if (ss.fail()) {
                throw std::runtime_error("Failed to parse date time string");
            }
            return std::chrono::system_clock::from_time_t(std::mktime(&tm));
        }

        template<typename T>
        static std::chrono::system_clock::time_point from_json_element(const T &json_element) {
            if (json_element.contains("dateTime")) {
                return from_string(json_element.at("dateTime"));
            } else if (json_element.contains("date")) {
                return from_dateonly_string(json_element.at("date"));
            }
            throw std::runtime_error("Failed to parse date time from json element");
        }
    };
}