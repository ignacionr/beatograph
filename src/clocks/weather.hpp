#pragma once

#include <format>
#include <string>
#include <stdexcept>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "../conversions/uri.hpp"

namespace weather {
    struct openweather_client {
        openweather_client(std::string_view api_key) : api_key_{api_key} {}

        static size_t write_callback(void *contents, size_t size, size_t nmemb, std::string *s) {
            size_t new_length = size * nmemb;
            size_t old_length = s->size();
            s->resize(old_length + new_length);
            std::copy((char *)contents, (char *)contents + new_length, s->begin() + old_length);
            return size * nmemb;
        }

        nlohmann::json::object_t get_weather(std::string_view city) {
            // https://api.openweathermap.org/data/2.5/weather?q={city name}&appid={API key}
            auto url = std::format("https://api.openweathermap.org/data/2.5/weather?q={}&appid={}", 
                conversions::uri::encode_component(city), 
                api_key_);
            // query using curl
            CURL *curl = curl_easy_init();
            if (curl) {
                std::string response;
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
                curl_easy_setopt(curl, CURLOPT_USERAGENT, "beat-o-graph/1.0");
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                auto res = curl_easy_perform(curl);
                if (res != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    throw std::runtime_error(curl_easy_strerror(res));
                }
                curl_easy_cleanup(curl);
                return nlohmann::json::parse(response);
            }
            else {
                throw std::runtime_error("Failed to initialize curl");
            }
        }

        std::string icon_url(std::string_view key) {
            return std::format("http://openweathermap.org/img/wn/{}@2x.png", key);
        }

        std::string icon_local_file(std::string_view key) {
            return std::format("assets/weather/{}@2x.png", key);
        }

    private:
        std::string api_key_;
    };
}