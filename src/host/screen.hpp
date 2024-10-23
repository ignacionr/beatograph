#pragma once

#include <numeric>
#include <ranges>
#include <imgui.h>
#include "host.hpp"
#include "../metrics/metric_view.hpp"

struct host_screen
{
    void render(host &host)
    {
        // Render host screen
        ImGui::LabelText("Hostname", "%s", host.name().c_str());
        ImGui::Indent();
        if (ImGui::CollapsingHeader("SSH Properties")) {
            auto interesting_keys = std::array<std::string_view, 5>{"host", "hostname", "user", "port", "identityfile"};
            auto filtered_properties = host.properties() |
                                    std::views::filter([&](auto const &pair)
                                                        { return std::ranges::find(interesting_keys, pair.first) != interesting_keys.end(); });
            for (auto const &[key, value] : filtered_properties) {
                ImGui::LabelText(key.c_str(), "%s", value.c_str());
            }
        }
        if (ImGui::CollapsingHeader("Performance Metrics")) {
            std::shared_ptr<metrics_model> model = host.metrics();
            if (model) {
                for (auto const &[metric, values] : model->metrics) {
                    metric_view_.render(metric, model->views[&metric], values);
                }
            }
        }
        ImGui::Unindent();
    }
private:
    metric_view metric_view_;
};
