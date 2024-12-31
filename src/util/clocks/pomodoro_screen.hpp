#pragma once

#include <chrono>
#include <cmath>
#include <format>
#include <memory>
#include <string>

#include <imgui.h>

#include "pomodoro.hpp"

namespace clocks {
    struct pomodoro_screen {
        pomodoro_screen(std::shared_ptr<pomodoro> pomodoro) : pomodoro_{pomodoro} {}

        void render()
        {
            if (ImGui::BeginChild("Pomodoro")) {
                if (ImGui::SmallButton(ICON_MD_RESET_TV " Reset")) {
                    pomodoro_->reset();
                }
                auto const current_time = std::chrono::system_clock::now();
                if (pomodoro_->is_done(current_time)) {
                    ImGui::TextUnformatted("Pomodoro done!");
                } else {
                    auto percentage = pomodoro_->percentaged_done(current_time);
                    auto dd = ImGui::GetWindowDrawList();
                    // make a circle
                    auto const pos {ImGui::GetWindowPos()};
                    auto const center = ImVec2 {
                        pos.x + ImGui::GetWindowWidth() / 2,
                        pos.y + ImGui::GetWindowHeight() / 2};
                    auto const radius = std::min(
                        0.45 * ImGui::GetWindowWidth(), 
                        0.45 * ImGui::GetWindowHeight());
                    // five spikes
                    for (int i = 0; i < 5; ++i) {
                        auto const angle = 2 * M_PI * i / 5 + M_PI / 2;
                        auto const spike = ImVec2 {
                            static_cast<float>(center.x + radius * std::cos(angle)),
                            static_cast<float>(center.y + radius * std::sin(angle))};
                        dd->AddLine(center, spike, IM_COL32(255, 255, 255, 255), 2);
                    }
                    // and now fill the percentage that corresponds to the time accrued
                    auto const angle = 2 * M_PI * percentage + M_PI / 2;
                    // first the parts that are completed
                    static constexpr double fifth_of_circle {2.0 * M_PI / 5.0};
                    for (auto angle_to {M_PI / 2.0 + fifth_of_circle}; angle_to <= angle ; angle_to += fifth_of_circle) {
                        auto const angle_from = angle_to - fifth_of_circle;
                        auto const spike_from = ImVec2 {
                            static_cast<float>(center.x + radius * std::cos(angle_from)),
                            static_cast<float>(center.y + radius * std::sin(angle_from))};
                        auto const spike_to = ImVec2 {
                            static_cast<float>(center.x + radius * std::cos(angle_to)),
                            static_cast<float>(center.y + radius * std::sin(angle_to))};
                        dd->AddTriangleFilled(center, spike_from, spike_to, IM_COL32(255, 200, 0, 255));
                    }
                    // then the fraction currently running
                    auto const reference_angle = std::floor((angle - M_PI / 2) / fifth_of_circle) * fifth_of_circle + M_PI / 2;
                    auto const spike = ImVec2 {
                        static_cast<float>(center.x + radius * std::cos(angle)),
                        static_cast<float>(center.y + radius * std::sin(angle))};
                    auto const reference_spike = ImVec2 {
                        static_cast<float>(center.x + radius * std::cos(reference_angle)),
                        static_cast<float>(center.y + radius * std::sin(reference_angle))};
                    dd->AddTriangleFilled(center, spike, reference_spike, IM_COL32(255, 200, 0, 220));
                }
            }
            ImGui::EndChild();
        }
    private:
        std::shared_ptr<pomodoro> pomodoro_;
    };
}