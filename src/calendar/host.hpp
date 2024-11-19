#pragma once

#include <chrono>
#include <format>
#include <ranges>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "../conversions/date_time.hpp"

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

        static size_t write_string(void *ptr, size_t size, size_t nmemb, std::string *data) {
            data->append(static_cast<char *>(ptr), size * nmemb);
            return size * nmemb;
        }

        event_set get_events() const {
            CURL *curl = curl_easy_init();
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, delegate_url_.c_str());
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string);
                std::string response;
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                auto cr = curl_easy_perform(curl);
                if (cr != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    throw std::runtime_error(std::format("Error: {}", curl_easy_strerror(cr)));
                }
                curl_easy_cleanup(curl);
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
            throw std::runtime_error("Error: curl_easy_init failed.");
        }
    private:
        std::string delegate_url_;
    };
}