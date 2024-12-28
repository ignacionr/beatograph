#pragma once

#include <chrono>
#include <format>
#include <ranges>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../../hosting/http/fetch.hpp"
#include "../../util/conversions/date_time.hpp"

namespace calendar {

    struct entry {
        std::string summary;
        std::string description;
        std::string location;
        std::chrono::system_clock::time_point start;
        std::chrono::system_clock::time_point end;
    };

    struct event_set {
        using entries_t = std::vector<entry>;
        entries_t entries;

        auto in_range(std::chrono::system_clock::time_point const start, std::chrono::system_clock::time_point const end) const {
            return entries | std::ranges::views::filter([start,end](auto const &entry) {
                return 
                    (entry.start <= end && entry.end >= start);
            });
        }
    };

    struct host {
        host(std::string_view delegate_url): delegate_url_{delegate_url} {}

        event_set get_events() const {
            auto response = http::fetch()(delegate_url_);
            auto doc = nlohmann::json::parse(response);
            event_set result;
            for (auto const &it: doc.at("items").get<nlohmann::json::array_t>()) {
                result.entries.push_back(entry{
                    .summary = it.at("summary").get<std::string>(),
                    .description = it.contains("description") ? it.at("description").get<std::string>() : std::string{},
                    .location = it.contains("location") ? it.at("location").get<std::string>() : std::string{},
                    .start = conversions::date_time::from_json_element(it.at("start")),
                    .end = conversions::date_time::from_json_element(it.at("end"))
                });
            }
            return result;
        }
    private:
        std::string delegate_url_;
    };
}