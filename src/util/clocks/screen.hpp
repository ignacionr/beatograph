#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <math.h>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "../../structural/views/cached_view.hpp"
#include "../../structural/views/json.hpp"
#include "../../imgcache.hpp"
#include "../../registrar.hpp"
#include "../external/cppgpt/cppgpt.hpp"
#include "host.hpp"

namespace clocks
{
    struct screen
    {
        screen(std::shared_ptr<host> h, std::function<bool()> quitting, ignacionr::cppgpt &&gpt, std::function<void(std::string_view)> notify)
            : host_{h}, gpt_{gpt}, notify_{notify}
        {
            refresh_ = std::jthread([this, quitting]
                                    {
                                        while (!quitting()) {
                                            auto all_cities_vector = host_->get_cities();
                                            for (auto &city : *all_cities_vector) {
                                                try
                                                {
                                                    nlohmann::json result = host_->get_weather(city.label);
                                                    {
                                                        city.weather_info(result);
                                                    }
                                                }
                                                catch (std::exception const &e)
                                                {
                                                    notify_(e.what());
                                                }
                                                std::this_thread::sleep_for(std::chrono::seconds(3));
                                                if (quitting()) {
                                                    break;
                                                }
                                            }
                                            for (int i = 0; i < 130 && !quitting(); ++i) {
                                                std::this_thread::sleep_for(std::chrono::seconds(1));
                                            }
                                        }
                                    });
        }

        static ImTextureID clock_background()
        {
            static ImTextureID texture_id = nullptr;
            if (texture_id == nullptr)
            {
                constexpr int width = 500;
                constexpr int height = 500;
                std::array<unsigned char, width * height * 4> pixels;

                constexpr auto sqr_radius = (width / 2) * (width / 2);
                constexpr auto sqr_radius_low = (width * 9 / 20) * (width * 9 / 20);
                auto p = &pixels.front();
                for (int y{}; y < height; ++y) {
                    for (int x{}; x < width; ++x) {
                        auto const d{(x - width / 2) * (x - width / 2) + (y - height / 2) * (y - height / 2)};
                        bool const in_circle_border = (d < sqr_radius) && (d > sqr_radius_low);
                        *p++ = 255 * in_circle_border;
                        *p++ = 128 * in_circle_border;
                        *p++ = 0;
                        *p++ = 70 * in_circle_border;
                    }
                }

                for (float a = 0.0f; a < 2 * M_PIf; a += M_PIf / 6.0f)
                {
                    auto const x = static_cast<int>(width / 2 + width * 9  * std::cos(a) / 20);
                    auto const y = static_cast<int>(height / 2 + height * 9 * std::sin(a) / 20);
                    for (int i = -5; i <= 5; ++i)
                    {
                        for (int j = -5; j <= 5; ++j)
                        {
                            auto const idx = 4 * (y * width + x + i + j * width);
                            pixels[idx] = 255;
                            pixels[idx + 1] = 255;
                            pixels[idx + 2] = 128;
                            pixels[idx + 3] = 128;
                        }
                    }
                }

                GLuint texture;
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, 0);

                texture_id = (ImTextureID)(intptr_t)texture;
            }
            return texture_id;
        }

        void render_clock(std::string_view label,
                          std::chrono::system_clock::time_point time, bool show_seconds)
        {
            auto const et = time.time_since_epoch();
            ImGui::TextUnformatted(std::format("\n{:02}:{:02}",
                                               std::chrono::floor<std::chrono::hours>(et).count() % 24,
                                               std::chrono::floor<std::chrono::minutes>(et).count() % 60)
                                       .c_str());
            auto const startY = ImGui::GetCursorPosY() + ImGui::GetWindowPos().y;
            auto const current_milli = std::chrono::floor<std::chrono::milliseconds>(time);
            auto const met = current_milli.time_since_epoch();
            auto const minute_angle = 2 * M_PI * ((met.count() / 1000) % 3600) / 3600.0 - M_PI / 2.0;
            auto const hour{(met.count() / 1000) % 43200};
            auto const hour_angle = 2 * M_PI * (hour) / 43200.0 - M_PI / 2.0;
            auto dl = ImGui::GetWindowDrawList();
            auto const center = ImVec2{
                ImGui::GetWindowPos().x + ImGui::GetWindowWidth() / 2,
                startY + ImGui::GetWindowWidth() / 2};
            auto const radius = std::min(ImGui::GetWindowWidth(), ImGui::GetWindowHeight()) / 2 - 5;
            auto const hour_hand = ImVec2{
                center.x + static_cast<float>(radius * 0.4 * std::cos(hour_angle)),
                center.y + static_cast<float>(radius * 0.4 * std::sin(hour_angle))};
            auto const minute_hand = ImVec2{
                center.x + static_cast<float>(radius * 0.7 * std::cos(minute_angle)),
                center.y + static_cast<float>(radius * 0.7 * std::sin(minute_angle))};
            ImGui::TextUnformatted(label.data());
            dl->AddImage(clock_background(), 
                ImVec2{center.x - radius, center.y - radius}, 
                ImVec2{center.x + radius, center.y + radius});
            dl->AddLine(center, hour_hand,
                        IM_COL32(255, 255, 255, 255),
                        5);
            dl->AddLine(center, minute_hand,
                        IM_COL32(255, 255, 255, 255),
                        2);
            if (show_seconds)
            {
                auto const second_angle = 2 * M_PI * (current_milli.time_since_epoch().count() % 60000) / 60000.0 - M_PI / 2.0;
                auto const second_hand = ImVec2{
                    center.x + static_cast<float>(radius * 0.9 * std::cos(second_angle)),
                    center.y + static_cast<float>(radius * 0.9 * std::sin(second_angle))};
                dl->AddLine(center, second_hand, IM_COL32(255, 128, 0, 255), 2);
            }
        }

        void render()
        {
            constexpr auto side_width{210};
            constexpr auto cell_height{340};
            int column_count = static_cast<int>(ImGui::GetWindowWidth()) / (side_width + 10);
            if (column_count > 0 &&
                ImGui::BeginTable("Clocks", column_count, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg, ImVec2{ImGui::GetWindowWidth() - 20, ImGui::GetWindowHeight() - 20}))
            {
                ImGui::TableNextColumn();
                ImGui::BeginChild("UTC", ImVec2{side_width, cell_height});
                render_clock("UTC", std::chrono::system_clock::now(), true);
                ImGui::EndChild();
                {
                    auto all_cities_vector = host_->get_cities();
                    for (clocks::city_info const &city : *all_cities_vector)
                    {
                        ImGui::TableNextColumn();
                        ImGui::BeginChild(city.label.data(), ImVec2{side_width, cell_height});
                        if (city.has_weather_info())
                        {
                            auto const start_y{ImGui::GetCursorPosY()};
                            ImGui::TextUnformatted("\n\n\n\n");
                            auto const &local_icon = host_->icon_local_file(city.weather_icon);
                            auto const texture{cache_.load_texture_from_file(local_icon)};
                            auto const name = city.label;
                            auto const tz = city.timezone;
                            auto color = city.feels_like > 30.0f ? ImVec4{1.0f, 0.0f, 0.0f, 1.0f} : (city.feels_like < 16.0f ? ImVec4{0.5f, 0.5f, 1.0f, 1.0f} : ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
                            ImGui::PushStyleColor(ImGuiCol_Text, color);
                            ImGui::TextUnformatted(city.temperature_text.data(), city.temperature_text.data() + city.temperature_text.size());
                            ImGui::PopStyleColor();
                            ImGui::TextUnformatted(city.humidity_text.data(), city.humidity_text.data() + city.humidity_text.size());
                            auto sun_times = std::format(ICON_MD_ARROW_CIRCLE_UP " {}:{:02} " ICON_MD_ARROW_CIRCLE_DOWN " {}:{:02}",
                                                         city.sunrise_seconds / 3600 % 24,
                                                         city.sunrise_seconds / 60 % 60,
                                                         city.sunset_seconds / 3600 % 24,
                                                         city.sunset_seconds / 60 % 60);
                            ImGui::TextUnformatted(sun_times.data(), sun_times.data() + sun_times.size());
                            ImGui::TextUnformatted(city.main_weather.data(), city.main_weather.data() + city.main_weather.size());
                            if (show_details_)
                            {
                                json_view_.render(city.weather_info());
                            }
                            ImGui::Image(reinterpret_cast<void *>(static_cast<uintptr_t>(texture)), ImVec2{90, 90});
                            auto const restore_y{ImGui::GetCursorPosY()};
                            ImGui::SetCursorPosY(start_y);
                            auto const local_time {std::chrono::system_clock::now() + std::chrono::seconds{tz}};
                            render_clock(name, local_time, false);
                            ImGui::SetCursorPosY(restore_y);
                            if (ImGui::SmallButton("Announce")) {
                                try {
                                    gpt_.add_instructions(std::format("You are a senior weather forecaster with a long experience in interpreting weather data. "
                                        "You will be presented with a JSON object representing the current conditions of a place. "
                                        "Do not use markdown, since this is a text-to-speech application. "
                                        "Use exclusively metric units. "
                                        "If there are upcoming local holidays, mention it together with a short reference to what is celebrated. "
                                        "The local date and time is {:%Y-%m-%d %H:%M:%S}", local_time)
                                    );
                                    std::string const result = gpt_.sendMessage(city.weather_info().dump(), "user", "llama3-groq-70b-8192-tool-use-preview").at("choices").at(0).at("message").at("content");
                                    notify_(result);
                                }
                                catch(std::exception &e) {
                                    notify_(e.what());
                                }
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
            if (item == "View")
            {
                ImGui::SeparatorText("Clocks and Weather");
                if (ImGui::MenuItem("Show Details", nullptr, show_details_))
                {
                    show_details_ = !show_details_;
                }
            }
        }

    private:
        std::shared_ptr<host> host_;
        img_cache &cache_ = *registrar::get<img_cache>({});
        views::json json_view_;
        bool show_details_{false};
        std::jthread refresh_;
        ignacionr::cppgpt gpt_;
        std::function<void(std::string_view)> notify_;
    };
}