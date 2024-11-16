#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <math.h>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "../views/cached_view.hpp"
#include "../views/json.hpp"
#include "weather.hpp"
#include "../imgcache.hpp"

namespace clocks
{
    struct screen
    {
        screen(weather::openweather_client &weather_client, img_cache &cache)
            : weather_client_{weather_client}, cache_{cache} {
            refresh_ = std::jthread([this]
            {
                while (!quit_) {
                for (auto &city : all_cities) {
                    try {
                        nlohmann::json result = weather_client_.get_weather(city.label);
                        {
                            std::lock_guard lock{city_mutex_};
                            city.weather_info = result;
                        }
                    }
                    catch (std::exception const &e) {
                        std::cerr << "Error fetching weather: " << e.what() << std::endl;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(7));
                }
            } });
        }
        ~screen()
        {
            quit_ = true;
        }

        void render_clock(std::string_view label,
                          std::chrono::system_clock::time_point time, bool show_seconds)
        {
            ImGui::TextUnformatted(std::format("\n{:02}:{:02}",
                                               std::chrono::floor<std::chrono::hours>(time.time_since_epoch()).count() % 24,
                                               std::chrono::floor<std::chrono::minutes>(time.time_since_epoch()).count() % 60)
                                       .c_str());
            auto const startY = ImGui::GetCursorPosY() + ImGui::GetWindowPos().y;
            auto const current_second = std::chrono::floor<std::chrono::seconds>(time);
            auto const minute_angle = 2 * M_PI * (current_second.time_since_epoch().count() % 3600) / 3600.0 - M_PI / 2.0;
            auto const hour{current_second.time_since_epoch().count() % 43200};
            auto const hour_angle = 2 * M_PI * (hour) / 43200.0 - M_PI / 2.0;
            auto dl = ImGui::GetWindowDrawList();
            auto const center = ImVec2 {
                ImGui::GetWindowPos().x + ImGui::GetWindowWidth() / 2,
                startY + ImGui::GetWindowWidth() / 2};
            auto const radius = std::min(ImGui::GetWindowWidth(), ImGui::GetWindowHeight()) / 2 - 5;
            auto const hour_hand = ImVec2 {
                center.x + static_cast<float>(radius * 0.4 * std::cos(hour_angle)),
                center.y + static_cast<float>(radius * 0.4 * std::sin(hour_angle))};
            auto const minute_hand = ImVec2 {
                center.x + static_cast<float>(radius * 0.7 * std::cos(minute_angle)),
                center.y + static_cast<float>(radius * 0.7 * std::sin(minute_angle))};
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
                auto const second_angle = 2 * M_PI * (current_second.time_since_epoch().count() % 60) / 60.0 - M_PI / 2.0;
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
            constexpr auto side_width{120};
            constexpr auto cell_height{250};
            int column_count = static_cast<int>(ImGui::GetWindowWidth()) / (side_width + 10);
            if (ImGui::BeginTable("Clocks", column_count, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg, ImVec2{ImGui::GetWindowWidth() - 20, ImGui::GetWindowHeight() - 20}))
            {
                ImGui::TableNextColumn();
                ImGui::BeginChild("UTC", ImVec2{side_width, cell_height});
                render_clock("UTC", std::chrono::system_clock::now(), true);
                ImGui::EndChild();
                {
                    std::lock_guard lock{city_mutex_};
                    for (auto const &city : all_cities)
                    {
                        ImGui::TableNextColumn();
                        ImGui::BeginChild(city.label.data(), ImVec2{side_width, cell_height});
                        if (city.weather_info.is_object() && city.weather_info.contains("weather"))
                        {
                            auto const &weather = city.weather_info["weather"][0];
                            auto const local_icon = weather_client_.icon_local_file(weather["icon"]);
                            auto const texture{cache_.load_texture_from_file(local_icon)};
                            render_clock(city.label, std::chrono::system_clock::now() + std::chrono::seconds{city.weather_info.at("timezone").get<int>()}, false);
                            ImGui::Image(reinterpret_cast<void *>(static_cast<uintptr_t>(texture)), ImVec2{50, 50});
                            ImGui::TextUnformatted(std::format("{:.1f}°C", city.weather_info["main"]["temp"].get<double>() - 273.15).c_str());
                            if (show_details_) {
                                json_view_.render(city.weather_info);
                            }
                        }
                        else
                        {
                            render_clock(city.label, {}, false);
                        }
                        ImGui::EndChild();
                    }
                }
                ImGui::EndTable();
            }
        }

        void render_menu(std::string_view item)
        {
            if (item == "View") {
                ImGui::SeparatorText("Clocks and Weather");
                if (ImGui::MenuItem("Show Details", nullptr, show_details_))
                {
                    show_details_ = !show_details_;
                }
            }
        }

    private:
        weather::openweather_client &weather_client_;
        img_cache &cache_;
        bool quit_{false};
        views::json json_view_;
        bool show_details_{false};
        std::vector<city_info> all_cities{{{"Los Angeles"},
                                                    {"New York"},
                                                    {"Buenos Aires"},
                                                    {"Santa Fe, AR"},
                                                    {"Rome, IT"},
                                                    {"Moscow"},
                                                    {"Bangkok"},
                                                    {"Pattaya, TH"}
                                                    }};
        std::jthread refresh_;
        std::mutex city_mutex_;
    };
}