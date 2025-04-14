#include "pch.h"
#include "metric_view_config_screen.hpp"

void metric_view_config_screen::render() noexcept
{
    ImGui::InputFloat("Min value", &config.min_value);
    ImGui::InputFloat("Max value", &config.max_value);
    ImGui::InputFloat("Concern threshold", &config.concern_threshold);
    ImGui::InputFloat("Warning threshold", &config.warning_threshold);
    ImGui::ColorEdit4("Normal color", config.normal_color);
    ImGui::ColorEdit4("Warning color", config.warning_color);
    ImGui::ColorEdit4("Concern color", config.concern_color);
}
