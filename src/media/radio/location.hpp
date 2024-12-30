#pragma once

#include <chrono>
#include <format>
#include <optional>
#include <string>

#include <imgui.h>

namespace media::radio
{
    struct location
    {
        std::optional<float> render(std::chrono::milliseconds total_run, 
            std::chrono::milliseconds current_run) 
        {
            std::optional<float> result;
            float progress = dial_x.value_or(total_run.count() == 0 ? 
                1.0f :
                static_cast<float>(static_cast<float>(current_run.count()) / total_run.count()));
            auto const target_run = std::chrono::milliseconds(
                dial_x.has_value() ? 
                static_cast<long long>(dial_x.value() * total_run.count()) : current_run.count());
            auto current_run_str = std::format(" {:02}:{:02}\n/{:02}:{:02}", 
                std::chrono::duration_cast<std::chrono::minutes>(target_run).count(),
                std::chrono::duration_cast<std::chrono::seconds>(target_run).count() % 60,
                std::chrono::duration_cast<std::chrono::minutes>(total_run).count(),
                std::chrono::duration_cast<std::chrono::seconds>(total_run).count() % 60);

            auto draw_list = ImGui::GetWindowDrawList();
            static constexpr float max_thickness = 26.0f;
            static constexpr float reel_radius = 11.0f;
            static constexpr float M_2PIf = 6.28318530717958647692528676655900576f;
            float const to_thickness {progress * max_thickness};
            float const from_thickness {max_thickness - to_thickness};
            ImVec2 const center_from {ImGui::GetCursorPosX() + 50, ImGui::GetCursorPosY() + 64};
            ImVec2 const center_to {ImGui::GetCursorPosX() + 120, ImGui::GetCursorPosY() + 64};

            // reel marks
            for (int i = 0; i < 8; i++)
            {
                auto angle = i * M_2PIf / 8.0f - (M_2PIf * max_thickness) * progress;
                float const x1 = reel_radius * cos(angle);
                float const y1 = reel_radius * sin(angle);
                auto const min = ImVec2{center_from.x + x1 - 1, center_from.y + y1 - 1};
                auto const max = ImVec2{center_from.x + x1 + 1, center_from.y + y1 + 1};
                draw_list->AddRectFilled(min, max, IM_COL32(255, 255, 255, 128));
                auto const min2 = ImVec2{center_to.x + x1 - 1, center_to.y + y1 - 1};
                auto const max2 = ImVec2{center_to.x + x1 + 1, center_to.y + y1 + 1};
                draw_list->AddRectFilled(min2, max2, IM_COL32(255, 255, 255, 128));
            }

            // the reels
            draw_list->AddCircle(center_from, reel_radius + from_thickness / 2.0f, IM_COL32(255, 255, 0, 255), 36, from_thickness);
            draw_list->AddCircle(center_to, reel_radius + to_thickness / 2.0f, IM_COL32(255, 255, 0, 255), 36, to_thickness);

            // knobs
            draw_list->AddCircleFilled({center_from.x - 30.0f, center_from.y + 33.0f}, 4.0f, IM_COL32(255, 255, 0, 255), 6);
            draw_list->AddCircleFilled({center_to.x + 30.0f, center_to.y + 33.0f}, 4.0f, IM_COL32(255, 255, 0, 255), 6);

            // the tape
            draw_list->AddLine(
                {center_from.x - reel_radius - from_thickness, center_from.y}, 
                {center_from.x - 32.0f, center_from.y + 30.0f}, 
                IM_COL32(255, 255, 0, 255));
            draw_list->AddLine(
                {center_to.x + reel_radius + to_thickness, center_to.y}, 
                {center_to.x + 32.0f, center_to.y + 30.0f}, 
                IM_COL32(255, 255, 0, 255));

            // cover top and bottom
            draw_list->AddRectFilled({ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 20}, {ImGui::GetCursorPosX() + 180, ImGui::GetCursorPosY() + 45}, IM_COL32(0, 0, 0, 170));
            draw_list->AddRectFilled({ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 85}, {ImGui::GetCursorPosX() + 180, ImGui::GetCursorPosY() + 101}, IM_COL32(0, 0, 0, 170));
            
            // draw progress
            auto const start_x {ImGui::GetCursorPosX() + 170};
            auto const start_y {ImGui::GetCursorPosY() + 20};
            ImGui::SetCursorPosX(start_x);
            auto const width_x {ImGui::GetWindowWidth() - start_x};
            ImGui::ProgressBar(progress, {-1, 70}, current_run_str.c_str());
            if (ImGui::IsMouseHoveringRect({start_x, start_y}, {start_x + width_x, start_y + 70})) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left) ) {
                    dial_x = (ImGui::GetMousePos().x - start_x - 6) / width_x;
                }
                else if (dial_x.has_value()) {
                    result = dial_x;
                    dial_x.reset();
                }
            }
            return result;
        }
        std::optional<float> dial_x;
    };
}