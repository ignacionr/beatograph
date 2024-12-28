#pragma once

#include <format>
#include <string>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "../conversions/uri.hpp"
#include "../../hosting/http/fetch.hpp"

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
            auto const contents{http::fetch()(url)};
            auto result {nlohmann::json::parse(contents)};
            return result;
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