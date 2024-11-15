#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <math.h>

#include <imgui.h>

namespace clocks {
    struct screen {
        void render_clock(std::string_view label, std::chrono::system_clock::time_point time, bool show_seconds) {
            auto const current_second = std::chrono::floor<std::chrono::seconds>(time);
            auto const second_angle = 2 * M_PI * (current_second.time_since_epoch().count() % 60) / 60.0 - M_PI / 2.0;
            auto const minute_angle = 2 * M_PI * (current_second.time_since_epoch().count() % 3600) / 3600.0 - M_PI / 2.0;
            auto const hour {current_second.time_since_epoch().count() % 43200};
            auto const hour_angle = 2 * M_PI * (hour) / 43200.0 - M_PI / 2.0;
            auto dl = ImGui::GetWindowDrawList();
            auto const center = ImVec2 {
                ImGui::GetWindowPos().x + ImGui::GetWindowWidth() / 2, 
                ImGui::GetWindowPos().y + ImGui::GetWindowHeight() / 2};
            auto const radius = std::min(ImGui::GetWindowWidth(), ImGui::GetWindowHeight()) / 2 - 5;
            auto const hour_hand = ImVec2{
                center.x + static_cast<float>(radius * 0.4 * std::cos(hour_angle)), 
                center.y + static_cast<float>(radius * 0.4 * std::sin(hour_angle))
            };
            auto const minute_hand = ImVec2{
                center.x + static_cast<float>(radius * 0.7 * std::cos(minute_angle)), 
                center.y + static_cast<float>(radius * 0.7 * std::sin(minute_angle))
            };
            bool const pm {hour > 12};
            ImGui::TextUnformatted(pm ? "PM" : "AM");
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
            if (show_seconds) {
                auto const second_hand = ImVec2{
                    center.x + static_cast<float>(radius * 0.9 * std::cos(second_angle)), 
                    center.y + static_cast<float>(radius * 0.9 * std::sin(second_angle))
                };
                dl->AddLine(center, second_hand, IM_COL32(255, 0, 0, 255), 2);
            }
        }

        struct clock_t {
            std::string_view label;
            std::chrono::system_clock::duration diff;
        };

        void render() {
            static const std::array<clock_t, 6> all_clocks {{
                {"Los Angeles", std::chrono::hours(-8)},
                {"New York", std::chrono::hours(-5)},
                {"Buenos Aires", std::chrono::hours(-3)},
                {"UTC", std::chrono::minutes(0)},
                {"Moscow", std::chrono::hours(3)},
                {"Bangkok", std::chrono::hours(7)}
            }};
            int idx{0};
            constexpr auto side_width {120};
            int const per_row {static_cast<int>(ImGui::GetWindowWidth() / side_width - 0.5)};
            for (auto const &clock : all_clocks) {
                if (ImGui::BeginChild(clock.label.data(), ImVec2{side_width, side_width})) {
                    auto const now = std::chrono::system_clock::now();
                    render_clock(clock.label, now + clock.diff, idx == 0);
                }
                ImGui::EndChild();
                if (idx++ % per_row != (per_row - 1)){
                    ImGui::SameLine();
                }
            }
        }
    };
}