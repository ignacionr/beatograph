#pragma once

#include <chrono>
#include <string>

#include <imgui.h>

namespace media::radio
{
    struct location
    {
        void render(std::chrono::milliseconds total_run, 
            std::chrono::milliseconds current_run) 
        {
            double progress = total_run.count() == 0 ? 
                1.0 :
                static_cast<double>(current_run.count()) / total_run.count();
            ImGui::ProgressBar(static_cast<float>(progress));
        }
    };
}