#pragma once

#include <string>
#include <imgui.h>
#include "metric_view_config.hpp"

struct metric_view_config_screen {
    metric_view_config_screen(metric_view_config &config) : config(config) {}
    void render() {
        ImGui::InputFloat("Min value", &config.min_value);
        ImGui::InputFloat("Max value", &config.max_value);
        ImGui::InputFloat("Concern threshold", &config.concern_threshold);
        ImGui::InputFloat("Warning threshold", &config.warning_threshold);
        ImGui::ColorEdit4("Normal color", config.normal_color);
        ImGui::ColorEdit4("Warning color", config.warning_color);
        ImGui::ColorEdit4("Concern color", config.concern_color);
    }
private:
    metric_view_config &config;
};
