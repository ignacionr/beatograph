#pragma once

#include <memory>
#include "metric_view.hpp"
#include "metrics_menu.hpp"
#include "metrics_model.hpp"

struct metrics_screen {
    metrics_screen(metrics_model& model) : model{model}, menu{model} {}

    void render() {
        if (ImGui::BeginChild("MetricsMenu", ImVec2(ImGui::GetWindowWidth(), 230))) {
            menu.render();
            ImGui::EndChild();
        }

        if (menu.selected_metric != nullptr)
        {
            float const height{ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - 10};
            if (height > 0 && ImGui::BeginChild("MetricView", ImVec2(ImGui::GetWindowWidth() - 10, height)))
            {
                view.render(*menu.selected_metric,
                             model.views[menu.selected_metric],
                             model.metrics.at(*menu.selected_metric));
                ImGui::EndChild();
            }
        }
    }
private:    
    metric_view view;
    metrics_menu menu;
    metrics_model &model;
};
