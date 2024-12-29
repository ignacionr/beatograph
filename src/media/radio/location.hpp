#pragma once

#include <chrono>
#include <format>
#include <string>

#include <imgui.h>

namespace media::radio
{
    struct location
    {
        void render(std::chrono::milliseconds total_run, 
            std::chrono::milliseconds current_run) 
        {
            float progress = total_run.count() == 0 ? 
                1.0f :
                static_cast<float>(static_cast<float>(current_run.count()) / total_run.count());
            auto current_run_str = std::format(" {:02}:{:02}\n/{:02}:{:02}", 
                std::chrono::duration_cast<std::chrono::minutes>(current_run).count(),
                std::chrono::duration_cast<std::chrono::seconds>(current_run).count() % 60,
                std::chrono::duration_cast<std::chrono::minutes>(total_run).count(),
                std::chrono::duration_cast<std::chrono::seconds>(total_run).count() % 60);
            auto draw_list = ImGui::GetWindowDrawList();
            float const to_thickness {progress * 20.0f};
            float const from_thickness {20.0f - to_thickness};

            ImVec2 const center_from {ImGui::GetCursorPosX() + 50, ImGui::GetCursorPosY() + 68};
            ImVec2 const center_to {ImGui::GetCursorPosX() + 120, ImGui::GetCursorPosY() + 68};

            // reel marks
            for (int i = 0; i < 8; i++)
            {
                static constexpr float M_2PIf = 6.28318530717958647692528676655900576f;
                auto angle = i * M_2PIf / 8.0f - progress * 18.0f * M_2PIf;
                float const x1 = 14.0f * cos(angle);
                float const y1 = 14.0f * sin(angle);
                auto const min = ImVec2{center_from.x + x1 - 1, center_from.y + y1 - 1};
                auto const max = ImVec2{center_from.x + x1 + 1, center_from.y + y1 + 1};
                draw_list->AddRectFilled(min, max, IM_COL32(255, 255, 255, 128));
                auto const min2 = ImVec2{center_to.x + x1 - 1, center_to.y + y1 - 1};
                auto const max2 = ImVec2{center_to.x + x1 + 1, center_to.y + y1 + 1};
                draw_list->AddRectFilled(min2, max2, IM_COL32(255, 255, 255, 255));
            }

            // the reels
            draw_list->AddCircle(center_from, 15.0f + from_thickness / 2.0f, IM_COL32(255, 255, 0, 255), 24, from_thickness);
            draw_list->AddCircle(center_to, 15.0f + to_thickness / 2.0f, IM_COL32(255, 255, 0, 255), 24, to_thickness);

            // cover top and bottom
            draw_list->AddRectFilled({ImGui::GetCursorPosX(), ImGui::GetCursorPosY()}, {ImGui::GetCursorPosX() + 190, ImGui::GetCursorPosY() + 60}, IM_COL32(0, 0, 0, 170));
            draw_list->AddRectFilled({ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 80}, {ImGui::GetCursorPosX() + 190, ImGui::GetCursorPosY() + 140}, IM_COL32(0, 0, 0, 220));
            
            // draw progress
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 170);
            ImGui::ProgressBar(progress, {-1, 70}, current_run_str.c_str());
        }
    };
}