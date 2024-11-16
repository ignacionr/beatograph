#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <math.h>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "../views/cached_view.hpp"
#include "weather.hpp"
#include "../imgcache.hpp"

namespace clocks
{
    struct screen
    {
        screen(weather::openweather_client &weather_client, img_cache &cache)
            : weather_client_{weather_client}, cache_{cache} {}
        ~screen()
        {
            quit_ = true;
        }

        void render_clock(std::string_view label,
                          std::chrono::system_clock::time_point time, bool show_seconds)
        {
            auto const current_second = std::chrono::floor<std::chrono::seconds>(time);
            auto const second_angle = 2 * M_PI * (current_second.time_since_epoch().count() % 60) / 60.0 - M_PI / 2.0;
            auto const minute_angle = 2 * M_PI * (current_second.time_since_epoch().count() % 3600) / 3600.0 - M_PI / 2.0;
            auto const hour{current_second.time_since_epoch().count() % 43200};
            auto const hour_angle = 2 * M_PI * (hour) / 43200.0 - M_PI / 2.0;
            auto dl = ImGui::GetWindowDrawList();
            auto const center = ImVec2{
                ImGui::GetWindowPos().x + ImGui::GetWindowWidth() / 2,
                ImGui::GetWindowPos().y + ImGui::GetWindowWidth() / 2};
            auto const radius = std::min(ImGui::GetWindowWidth(), ImGui::GetWindowHeight()) / 2 - 5;
            auto const hour_hand = ImVec2{
                center.x + static_cast<float>(radius * 0.4 * std::cos(hour_angle)),
                center.y + static_cast<float>(radius * 0.4 * std::sin(hour_angle))};
            auto const minute_hand = ImVec2{
                center.x + static_cast<float>(radius * 0.7 * std::cos(minute_angle)),
                center.y + static_cast<float>(radius * 0.7 * std::sin(minute_angle))};
            ImGui::TextUnformatted(std::format("\n{:02}:{:02}",
                                               std::chrono::floor<std::chrono::hours>(time.time_since_epoch()).count() % 24,
                                               std::chrono::floor<std::chrono::minutes>(time.time_since_epoch()).count() % 60)
                                       .c_str());
            ImGui::TextUnformatted(label.data());
            dl->AddCircleFilled(center, radius,
                                IM_COL32(0, 0, 0, 100),
                                60);
            dl->AddLine(center, hour_hand,
                        IM_COL32(255, 255, 255, 255),
                        5);
            dl->AddLine(center, minute_hand,
                        IM_COL32(255, 255, 255, 255),
                        2);
            if (show_seconds)
            {
                auto const second_hand = ImVec2{
                    center.x + static_cast<float>(radius * 0.9 * std::cos(second_angle)),
                    center.y + static_cast<float>(radius * 0.9 * std::sin(second_angle))};
                dl->AddLine(center, second_hand, IM_COL32(255, 0, 0, 255), 2);
            }
        }

        struct city_info
        {
            std::string_view label;
            nlohmann::json weather_info;
        };

        void render()
        {
            static std::array<city_info, 6> all_cities{{{"Los Angeles"},
                                                        {"New York"},
                                                        {"Rome"},
                                                        {"Buenos Aires"},
                                                        {"Moscow"},
                                                        {"Bangkok"}}};
            static std::mutex city_mutex;
            static std::thread weather_thread{[&]
                                              {
                while (!quit_) {
                for (auto &city : all_cities) {
                    try {
                        nlohmann::json result = weather_client_.get_weather(city.label);
                        {
                            std::lock_guard lock{city_mutex};
                            city.weather_info = result;
                        }
                    }
                    catch (std::exception const &e) {
                        std::cerr << "Error fetching weather: " << e.what() << std::endl;
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(7));
            } }};
            constexpr auto side_width{120};
            auto const now = std::chrono::system_clock::now();
            if (ImGui::BeginChild("UTC", ImVec2{side_width, 0}))
            {
                render_clock("UTC", now, true);
            }
            ImGui::EndChild(); // UTC
            ImGui::SameLine();
            {
                std::lock_guard lock{city_mutex};
                for (auto const &city : all_cities)
                {
                    if (ImGui::BeginChild(city.label.data(), ImVec2{side_width, 0}))
                    {
                        if (city.weather_info.is_object() && city.weather_info.contains("weather"))
                        {
                            auto const &weather = city.weather_info["weather"][0];
                            auto const local_icon = weather_client_.icon_local_file(weather["icon"]);
                            auto const texture{cache_.load_texture_from_file(local_icon)};
                            ImGui::Image(
                                reinterpret_cast<void *>(
                                    static_cast<uintptr_t>(texture)),
                                ImVec2{50, 50});
                            ImGui::TextUnformatted(std::format("{:.1f}°C", city.weather_info["main"]["temp"].get<double>() - 273.15).c_str());
                            render_clock(city.label, now + std::chrono::seconds{city.weather_info.at("timezone").get<int>()}, false);
                        }
                        else
                        {
                            render_clock(city.label, now, false);
                        }
                    }
                    ImGui::EndChild();
                    ImGui::SameLine();
                }
            }
        }

    private:
        weather::openweather_client &weather_client_;
        img_cache &cache_;
        bool quit_{false};
    };
}