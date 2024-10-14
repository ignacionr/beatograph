#pragma once

#include <imgui.h>
#include "metric.hpp"
#include "metric_value.hpp"

struct metric_view {
    void render(metric const &m, auto const &values) {
        ImGui::Text("Name: %s", m.name.c_str());
        ImGui::Text("%s", m.help.c_str());
        for (metric_value const &v : values) {
            ImGui::Text("%s: %f", m.type.c_str(), v.value);
            for (auto const &[label_name, label_value] : v.labels) {
                ImGui::Text("\t%s: %s", label_name.c_str(), label_value.c_str());
            }
            ImGui::Separator();
        }
        // ImGui::Text("Value: %f", m.value);
    }
};