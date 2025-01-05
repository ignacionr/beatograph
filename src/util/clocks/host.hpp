#pragma once

#include <atomic>
#include <format>
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
        int timezone;
        std::string weather_icon;
        double temperature;
        double feels_like;
        std::string temperature_text;
        std::string humidity_text;
        int sunrise_seconds;
        int sunset_seconds;
        std::string main_weather;
        ImVec4 background_color{0.0f, 0.0f, 0.0f, 0.0f};

        city_info(std::string const &city) : label{city} {}

        void weather_info(nlohmann::json const &weather_info)
        {
            weather_info_ = weather_info;
            label = std::format("{}, {}",
                                                          weather_info["name"].get<std::string>(),
                                                          weather_info["sys"]["country"].get<std::string>());
            timezone = weather_info["timezone"].get<int>();
            weather_icon = weather_info["weather"][0]["icon"].get<std::string>();
            temperature = weather_info["main"]["temp"].get<double>() - 273.15;
            feels_like = weather_info["main"]["feels_like"].get<double>() - 273.15;
            temperature_text = std::format("{:.1f}C like {:.1f}C\n\n", temperature, feels_like);
            humidity_text = std::format("Humidity: {}%\n", weather_info["main"]["humidity"].get<int>());
            sunrise_seconds = weather_info["sys"]["sunrise"].get<int>() + timezone;
            sunset_seconds = weather_info["sys"]["sunset"].get<int>() + timezone;
            main_weather = weather_info["weather"][0]["main"].get<std::string>();
        }

        bool has_weather_info() const
        {
            return !weather_info_.empty();
        }

        auto const &weather_info() const
        {
            return weather_info_;
        }

    private:
        nlohmann::json weather_info_;
    };

    struct host {
        void add_city(std::string const &place_name, ImVec4 background_color = ImVec4{0.0f, 0.0f, 0.0f, 0.0f})
        {
            auto all_cities_vector = std::make_shared<std::vector<city_info>>(*all_cities.load());
            city_info city{place_name};
            city.background_color = background_color;
            all_cities_vector->emplace_back(city);
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