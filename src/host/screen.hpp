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
                auto available_bytes_pos = model->metrics.find({"node_memory_MemAvailable_bytes"});
                if (available_bytes_pos != model->metrics.end()) {
                    auto &available_bytes = available_bytes_pos->second;
                    auto total_bytes_pos = model->metrics.find({"node_memory_MemTotal_bytes"});
                    if (total_bytes_pos != model->metrics.end()) {
                        auto &total_bytes = total_bytes_pos->second;
                        auto available = std::accumulate(available_bytes.begin(), available_bytes.end(), 0.0, [](auto acc, auto const &mv) { return acc + mv.value; });
                        auto total = std::accumulate(total_bytes.begin(), total_bytes.end(), 0.0, [](auto acc, auto const &mv) { return acc + mv.value; });
                        if (total > 0.1) {
                            ImGui::ProgressBar(static_cast<float>(available / total), ImVec2(0.0f, 0.0f));
                            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                            ImGui::Text("Memory Usage: %.2f%%", 100.0 * available / total);
                        }
                    }
                }
                else {
                    ImGui::Text("Memory Usage: N/A");
                }
            }
            else {
                ImGui::Text("Metrics not available");
            }
        }
        ImGui::Unindent();
    }
private:
    metric_view metric_view_;
};
