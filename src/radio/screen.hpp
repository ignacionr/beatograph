#pragma once

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <SDL2/SDL.h>
#include "host.hpp"

namespace radio {
    struct screen {
        static constexpr auto dial_height{150};

        screen(host &h) : host_(h) {
            presets = host_.presets();
            std::transform(presets.begin(), presets.end(), std::back_inserter(presets_c_strs),
                           [](const auto &preset) { return preset.c_str(); });

        }
        ~screen() {
        }

        void render() {
            auto const dial_width = ImGui::GetWindowWidth();
            auto draw_list {ImGui::GetWindowDrawList()};
            auto const initial_pos = ImGui::GetCursorPos();

            constexpr auto green = IM_COL32(0, 160, 0, 255);
            // constexpr auto white = IM_COL32(255, 255, 255, 255);
            constexpr auto red = IM_COL32(255, 0, 0, 255);
            constexpr auto traslucid_gray = IM_COL32(128, 128, 128, 40);
            constexpr auto traslucid_yellow = IM_COL32(255, 255, 0, 40);

            auto const y_center = dial_height / 2;
            auto const selectable_height = dial_height / 7;
            std::array<float, 4> row_y {
                initial_pos.y + y_center - 2 * selectable_height - 10.0f, 
                initial_pos.y + y_center - selectable_height - 5.0f, 
                initial_pos.y + y_center + selectable_height - 5.0f, 
                initial_pos.y + y_center + 2 * selectable_height
            };

            // std::array<int, 4> row_x{};
            int current_row{1};
            auto const x_unit = dial_width / (presets.size() + 2);
            for (auto ip{0}; ip < presets.size(); ++ip) {
                auto const x = initial_pos.x + ip * x_unit;
                auto start_x = x + 2;
                draw_list->AddLine(
                    {start_x + 3, initial_pos.y + y_center - 1 + 2 * (current_row > 1)}, 
                    {start_x + 3, initial_pos.y + y_center - 4 + 8 * (current_row > 1)}, green);
                draw_list->AddLine(
                    {start_x + 4, initial_pos.y + y_center - 1 + 2 * (current_row > 1)}, 
                    {start_x + 4, initial_pos.y + y_center - 4 + 8 * (current_row > 1)}, green);
                draw_list->AddLine(
                    {start_x + 5, initial_pos.y + y_center - 1 + 2 * (current_row > 1)}, 
                    {start_x + 5, initial_pos.y + y_center - 4 + 8 * (current_row > 1)}, green);
                ImGui::SetCursorPosX(x);
                ImGui::SetCursorPosY(row_y[current_row]);
                if (ImGui::Selectable(presets[ip].c_str(), ip == currently_playing, 0, {3 * x_unit, selectable_height})) {
                    if (currently_playing != ip) {
                        host_.play(presets[ip]);
                        currently_playing = ip;
                    }
                    else {
                        host_.stop();
                        currently_playing = -1;
                    }
                }
                static constexpr int mapping[] = {3, 2, 0, 1};
                current_row = mapping[current_row];
            }
            auto target_x = currently_playing == -1 ? initial_pos.x : (initial_pos.x + currently_playing * x_unit + 2);
            if (dial_x < target_x) {
                dial_x = std::min(target_x, dial_x + (target_x - dial_x) / 10 + 1);
            }
            else if (dial_x > target_x) {
                dial_x = std::max(target_x, dial_x - (dial_x - target_x) / 10 - 1);
            }

            draw_list->AddLine({0, initial_pos.y + y_center - 1}, {dial_width, initial_pos.y + y_center - 1}, green);
            draw_list->AddLine({0, initial_pos.y + y_center + 1}, {dial_width, initial_pos.y + y_center + 1}, green);

            draw_list->AddTriangleFilled({dial_x + 1, initial_pos.y + 5}, {dial_x - 4, initial_pos.y}, {dial_x + 6, initial_pos.y}, red);
            draw_list->AddTriangleFilled({dial_x + 1, initial_pos.y + dial_height - 5}, {dial_x - 4, initial_pos.y + dial_height}, {dial_x + 6, initial_pos.y + dial_height}, red);
            draw_list->AddLine({dial_x, initial_pos.y}, {dial_x, initial_pos.y + dial_height}, red);
            draw_list->AddLine({dial_x + 2, initial_pos.y}, {dial_x + 2, initial_pos.y + dial_height}, red);

            draw_list->AddRectFilledMultiColor(
                {initial_pos.x, initial_pos.y + 3}, 
                {initial_pos.x + dial_width, initial_pos.y + dial_height - 6},
                traslucid_gray, traslucid_gray, traslucid_yellow, traslucid_yellow);

            ImGui::SetCursorPosX(initial_pos.x);
            ImGui::SetCursorPosY(initial_pos.y + dial_height + 10);

            if (host_.has_error()) {
                ImGui::Text("Error: %s", host_.last_error().c_str());
            }
        }
    private:
        std::vector<std::string> presets;
        std::vector<const char*> presets_c_strs;
        int currently_playing{-1};
        host &host_;
        float dial_x{};
    };
}