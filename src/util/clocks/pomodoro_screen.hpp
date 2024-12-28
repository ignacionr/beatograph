#pragma once

#include <chrono>
#include <cmath>
#include <format>
#include <string>

#include <imgui.h>

#include "pomodoro.hpp"

namespace clocks {
    struct pomodoro_screen {
        pomodoro_screen(pomodoro &pomodoro) : pomodoro_{pomodoro} {}

        void render_pomodoro()
        {
            auto const current_time = std::chrono::system_clock::now();
            if (pomodoro_.is_done(current_time)) {
                ImGui::TextUnformatted("Pomodoro done!");
            } else {
                auto percentage = pomodoro_.percentaged_done(current_time);
                auto dd = ImGui::GetWindowDrawList();
                // make a circle
                auto const center = ImVec2 {
                    ImGui::GetWindowPos().x + ImGui::GetWindowWidth() / 2,
                    ImGui::GetWindowPos().y + ImGui::GetWindowHeight() / 2};
                // five spikes
                for (int i = 0; i < 5; ++i) {
                    auto const angle = 2 * M_PI * i / 5 + M_PI / 2;
                    auto const spike = ImVec2 {
                        center.x + 0.9 * center.x * std::cos(angle),
                        center.y + 0.9 * center.y * std::sin(angle)};
                    dd->AddLine(center, spike, IM_COL32(255, 255, 255, 255), 2);
                }
                // and now fill the percentage that corresponds to the time accrued
                auto const angle = 2 * M_PI * percentage + M_PI / 2;
                auto const spike = ImVec2 {
                    center.x + 0.9 * center.x * std::cos(angle),
                    center.y + 0.9 * center.y * std::sin(angle)};
                dd->AddTriangle(center, spike, ImVec2{center.x, center.y + 0.9 * center.y}, IM_COL32(255, 255, 255, 255), 2);
                ImGui::TextUnformatted(std::format("{:.1f}%", percentage * 100).c_str());
            }
        }
    private:
        pomodoro &pomodoro_;
    };
}