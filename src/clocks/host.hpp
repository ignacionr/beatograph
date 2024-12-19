#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "weather.hpp"

namespace clocks {
    struct city_info
    {
        std::string label;
        nlohmann::json weather_info;
    };

    struct host {
        void add_city(std::string const &city)
        {
            auto all_cities_vector = std::make_shared<std::vector<city_info>>(*all_cities.load());
            all_cities_vector->push_back({city});
            all_cities.store(all_cities_vector);
        }
        auto get_cities() const
        {
            return all_cities.load();
        }
        nlohmann::json get_weather(std::string const &city) const
        {
            return weather_client_->get_weather(city);
        }
        void set_weather_client(std::shared_ptr<weather::openweather_client> client)
        {
            weather_client_ = client;
        }

        std::string const &icon_local_file(std::string key) {
            static std::unordered_map<std::string, std::string> cache;
            auto const it = cache.find(std::string{key});
            if (it == cache.end()) {
                cache[key] = std::format("assets/weather/{}@2x.png", key);
                return cache[key];
            }
            else {
                return it->second;
            }
        }

    private:
        std::atomic<std::shared_ptr<std::vector<city_info>>> all_cities {std::make_shared<std::vector<city_info>>()};
        std::shared_ptr<weather::openweather_client> weather_client_;
    };
}