#pragma once

#include <format>
#include <numeric>
#include <ranges>
#include <imgui.h>
#include "host.hpp"
#include "host_local.hpp"
#include "../metrics/metric_view.hpp"
#include "../docker/screen.hpp"

struct host_screen
{
    void render(host::ptr host, host_local &localhost)
    {
        if (ImGui::BeginChild(std::format("host-{}", host->name()).c_str(), {0, 0}, ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY))
        {
            // Render host screen
            ImGui::LabelText("Hostname", "%s", host->name().c_str());
            ImGui::Indent();
            if (ImGui::CollapsingHeader("SSH Properties"))
            {
                if (localhost.has_session(host->name())) {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
                    ImGui::TextUnformatted("Connected");
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    if (ImGui::Button("Recycle Session"))
                    {
                        localhost.recycle_session(host->name());
                    }
                }
                else {
                    ImGui::TextUnformatted("Disconnected");
                }
                auto interesting_keys = std::array<std::string_view, 5>{"host", "hostname", "user", "port", "identityfile"};
                auto filtered_properties = host->properties() |
                                           std::views::filter([&](auto const &pair)
                                                              { return std::ranges::find(interesting_keys, pair.first) != interesting_keys.end(); });
                for (auto const &[key, value] : filtered_properties)
                {
                    ImGui::LabelText(key.c_str(), "%s", value.c_str());
                }
            }
            if (ImGui::CollapsingHeader("Performance Metrics"))
            {
                std::shared_ptr<metrics_model> model = host->metrics(localhost);
                if (model)
                {
                    // Memory
                    auto available_bytes = model->sum("node_memory_MemAvailable_bytes");
                    auto total_bytes = model->sum("node_memory_MemTotal_bytes");
                    if (total_bytes.has_value() && available_bytes.has_value() && total_bytes.value() > 0)
                    {
                        double const ratio{1.0 - available_bytes.value() / total_bytes.value()};
                        ImGui::ProgressBar(static_cast<float>(ratio), ImVec2(0.0f, 0.0f));
                        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                        ImGui::Text("Memory Usage: %.2f%%", 100.0 * ratio);
                    }
                    else
                    {
                        ImGui::Text("Memory Usage: N/A");
                    }
                    // Disk
                    auto disk_free = model->sum("node_filesystem_free_bytes");
                    auto disk_total = model->sum("node_filesystem_size_bytes");
                    if (disk_total.has_value() && disk_free.has_value() && disk_total.value() > 0)
                    {
                        double const ratio{1.0 - disk_free.value() / disk_total.value()};
                        ImGui::ProgressBar(static_cast<float>(ratio), ImVec2(0.0f, 0.0f));
                        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                        ImGui::Text("Disk Usage: %.2f%%", 100.0 * ratio);
                    }
                    else
                    {
                        ImGui::Text("Disk Usage: N/A");
                    }
                }
                else
                {
                    ImGui::Text("Metrics not available");
                }
            }
            docker_screen_.render([host]() -> docker_host & { return host->docker(); }, localhost);
            if (ImGui::Button("Connect...")) {
                // just spawn a new process
                std::string command = std::format("ssh {}", host->name());
                // use the Windows API with a simple shell command
                if (auto result = reinterpret_cast<long long>(ShellExecuteA(NULL, "open", "cmd", std::format("/c {}", command).c_str(), NULL, SW_SHOW)); result <= 32) {
                    throw std::runtime_error(std::format("ShellExecute failed with error code {}", result));
                }
            }
            ImGui::Unindent();
        }
        ImGui::EndChild();
    }

private:
    metric_view metric_view_;
    docker_screen docker_screen_;
};
