#pragma once

#include <format>
#include <numeric>
#include <ranges>
#include <vector>

#include <imgui.h>

#include "host.hpp"
#include "host_local.hpp"
#include "../cloud/metrics/metric_view.hpp"
#include "../cloud/docker/screen.hpp"
#include "../structural/views/cached_view.hpp"

#include "../external/IconsMaterialDesign.h"

namespace hosting::ssh
{
    struct screen
    {
        void render(host::ptr host, std::shared_ptr<local::host> localhost) noexcept
        {
            if (!host)
            {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Disconnected.");
                return;
            }
            if (ImGui::BeginChild(std::format("host-{}", host->name()).c_str(), {0, 0}, 
                ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY))
            {
                // Render host screen
                ImGui::PushID(host->name().c_str());
                ImGui::LabelText("Hostname", "%s", host->name().c_str());
                ImGui::Indent();
                ImGui::Columns(3);
                if (ImGui::CollapsingHeader("SSH Properties"))
                {
                    if (localhost->has_session(host->name()))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
                        ImGui::TextUnformatted("Connected");
                        ImGui::PopStyleColor();
                        ImGui::SameLine();
                        if (ImGui::Button( ICON_MD_RECYCLING ))
                        {
                            localhost->recycle_session(host->name());
                        }
                    }
                    else
                    {
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
                ImGui::NextColumn();
                views::cached_view<std::string>("OS Release",
                    [host, localhost] {return host->get_os_release(localhost).c_str(); },
                    [](std::string const &data) { ImGui::TextUnformatted(data.c_str()); });
                ImGui::NextColumn();
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
                ImGui::NextColumn();
                ImGui::Columns();
                docker_screen_.render([host]() -> docker::host &
                                      { return host->docker(); }, localhost);
                struct unit
                {
                    unit (std::string_view line) {
                        auto parts = line | std::views::split(' ') | std::views::filter([](auto const &part) { return !part.empty(); });
                        auto it = parts.begin();
                        auto p = *it++;
                        name = std::string{p.begin(), p.end()};
                        p = *it++;
                        load = std::string{p.begin(), p.end()};
                        p = *it++;
                        active = std::string{p.begin(), p.end()};
                        p = *it++;
                        sub = std::string{p.begin(), p.end()};
                        description.clear();
                        for (p = *it++; it != parts.end(); p = *it++)
                        {
                            description += std::string{p.begin(), p.end()} + " ";
                        }
                    }

                    std::string name;
                    std::string load;
                    std::string active;
                    std::string sub;
                    std::string description;
                };
                views::cached_view<std::vector<unit>>("SystemCtl Units", [hostname = host->name(), localhost]() {
                    auto const result {localhost->ssh("sudo systemctl list-units", hostname)};
                    std::vector<unit> units;
                    for (auto const &line : std::string_view{result} | std::views::split('\n') | std::views::drop(1))
                    {
                        // break at an empty line
                        if (line.empty())
                        {
                            break;
                        }
                        units.push_back(std::string_view{line.begin(), line.end()});
                    }
                    return units;
                }, [this](std::vector<unit> const &units) {
                    if (systemctl_filter_.reserve(200); ImGui::InputText("Filter", systemctl_filter_.data(), systemctl_filter_.capacity()))
                    {
                        systemctl_filter_ = systemctl_filter_.data();
                    }

                    if (ImGui::BeginTable("SystemCtl Units", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
                    {
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("Load");
                        ImGui::TableSetupColumn("Active");
                        ImGui::TableSetupColumn("Sub");
                        ImGui::TableSetupColumn("Description");
                        ImGui::TableHeadersRow();
                        for (auto const &unit : units | std::views::filter([&](unit const &unit) { 
                            return unit.name.find(systemctl_filter_) != std::string::npos; }))
                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", unit.name.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", unit.load.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", unit.active.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", unit.sub.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", unit.description.c_str());
                        }
                        ImGui::EndTable();
                    }
                });

                struct process
                {
                    process(std::string_view line)
                    {
                        auto parts = line | std::views::split(' ') | std::views::filter([](auto const &part) { return !part.empty(); });
                        auto it = parts.begin();
                        auto p = *it++;
                        user = std::string{p.begin(), p.end()};
                        p = *it++;
                        pid = std::string{p.begin(), p.end()};
                        p = *it++;
                        cpu = std::string{p.begin(), p.end()};
                        p = *it++;
                        mem = std::string{p.begin(), p.end()};
                        p = *it++;
                        vsz = std::string{p.begin(), p.end()};
                        p = *it++;
                        rss = std::string{p.begin(), p.end()};
                        p = *it++;
                        tty = std::string{p.begin(), p.end()};
                        p = *it++;
                        stat = std::string{p.begin(), p.end()};
                        p = *it++;
                        start = std::string{p.begin(), p.end()};
                        p = *it++;
                        time = std::string{p.begin(), p.end()};
                        if (it != parts.end())
                        {
                            for (p = *it++; it != parts.end(); p = *it++)
                            {
                                command += std::string{p.begin(), p.end()} + " ";
                            }
                        }
                    }

                    std::string user;
                    std::string pid;
                    std::string cpu;
                    std::string mem;
                    std::string vsz;
                    std::string rss;
                    std::string tty;
                    std::string stat;
                    std::string start;
                    std::string time;
                    std::string command;
                };

                views::cached_view<std::vector<process>>("Processes", [hostname = host->name(), localhost]() {
                    auto const result {localhost->ssh("ps aux", hostname)};
                    std::vector<process> processes;
                    for (auto const &line : std::string_view{result} | std::views::split('\n') | std::views::drop(1))
                    {
                        // break at an empty line
                        if (line.empty())
                        {
                            break;
                        }
                        processes.push_back(std::string_view{line.begin(), line.end()});
                    }
                    return processes;
                }, [this](std::vector<process> const &processes) {
                    if (process_filter_.reserve(200); ImGui::InputText("Filter", process_filter_.data(), process_filter_.capacity()))
                    {
                        process_filter_ = process_filter_.data();
                    }

                    if (ImGui::BeginTable("Processes", 12, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
                    {
                        ImGui::TableSetupColumn("User");
                        ImGui::TableSetupColumn("PID");
                        ImGui::TableSetupColumn("CPU");
                        ImGui::TableSetupColumn("MEM");
                        ImGui::TableSetupColumn("VSZ");
                        ImGui::TableSetupColumn("RSS");
                        ImGui::TableSetupColumn("TTY");
                        ImGui::TableSetupColumn("STAT");
                        ImGui::TableSetupColumn("Start");
                        ImGui::TableSetupColumn("Time");
                        ImGui::TableSetupColumn("Command");
                        ImGui::TableHeadersRow();
                        for (auto const &process : processes | std::views::filter([&](process const &process) { 
                            return process.command.find(process_filter_) != std::string::npos; }))
                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(process.user.data(), process.user.data() + process.user.size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(process.pid.data(), process.pid.data() + process.pid.size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(process.cpu.data(), process.cpu.data() + process.cpu.size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(process.mem.data(), process.mem.data() + process.mem.size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(process.vsz.data(), process.vsz.data() + process.vsz.size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(process.rss.data(), process.rss.data() + process.rss.size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(process.tty.data(), process.tty.data() + process.tty.size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(process.stat.data(), process.stat.data() + process.stat.size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(process.start.data(), process.start.data() + process.start.size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(process.time.data(), process.time.data() + process.time.size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(process.command.data(), process.command.data() + process.command.size());
                        }
                        ImGui::EndTable();
                    }
                });
                ImGui::Unindent();
                if (ImGui::Button("Connect..."))
                {
                    last_error_.clear();
                    // just spawn a new process
                    std::string command = std::format("ssh {}", host->name());
                    // use the Windows API with a simple shell command
                    if (auto result = reinterpret_cast<long long>(ShellExecuteA(NULL, "open", "cmd", std::format("/c {}", command).c_str(), NULL, SW_SHOW)); result <= 32)
                    {
                        last_error_ = std::format("ShellExecute failed with error code {}", result);
                    }
                }
                if (!last_error_.empty())
                {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", last_error_.c_str());
                }
                ImGui::Unindent();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

    private:
        metric_view metric_view_;
        docker_screen docker_screen_;
        std::string systemctl_filter_;
        std::string process_filter_;
        std::string last_error_;
    };
}