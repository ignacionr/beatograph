#pragma once

#include <functional>
#include <ranges>
#include <imgui.h>
#include "host.hpp"
#include "../host/host.hpp"
#include "../host/host_local.hpp"

struct docker_screen {
    void render(auto getter, host_local &localhost)
    {
        if (ImGui::CollapsingHeader("Containers")) {
            auto& host{getter()};
            auto const &ps = host.ps();
            if (ps) {
                auto const &array {ps->get<nlohmann::json::array_t>()};
                if (array.empty()) {
                    ImGui::Text("No containers.");
                }
                else {
                    ImGui::Checkbox("Only running", &only_running);
                    // get the columns from the first container
                    auto cols = array.front().items() | 
                        std::views::transform([](auto const &item) { return item.key(); });
                    ImGui::BeginTable("Docker Containers", 
                        static_cast<int>(std::ranges::distance(cols)) + 1, 
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | 
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable);
                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                    for(auto const &column_name: cols) {
                        ImGui::Text("%s", column_name.c_str());
                        ImGui::TableSetColumnIndex(ImGui::TableGetColumnIndex() + 1);
                    }
                    for (auto const &container : array) {
                        if (container.contains("State") && container["State"].get<std::string>() == "running") {
                            ImGui::TableNextRow();
                            if (!only_running) {
                                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(0, (ImGui::TableGetRowIndex() %2 == 0) ? 0xd0 : 0xa0, 0, 255));
                            }
                        }
                        else if (only_running) {
                            continue;
                        }
                        else {
                            ImGui::TableNextRow();
                        }
                        for (auto const&[key, value]: container.items()) {
                            ImGui::Text("%s", value.get<std::string>().c_str());
                            ImGui::TableSetColumnIndex(ImGui::TableGetColumnIndex() + 1);
                        }
                        if (ImGui::Button("Shell")) {
                            host.open_shell(container["ID"].get<std::string>(), localhost);
                        }
                        if (ImGui::Button("Logs")) {
                            host.open_logs(container["ID"].get<std::string>());
                        }
                    }
                    ImGui::EndTable();
                }
            }
            else {
                ImGui::Text("N/A");
            }
            if (ImGui::Button("Refresh")) {
                auto t = std::thread([&localhost, &host] {
                    try {
                        host.fetch_ps(localhost);
                    }
                    catch(std::exception const &e) {
                        std::cerr << "Error: " << e.what() << std::endl;
                    }
                });
                t.detach();
            }
        }
    }

    bool only_running{true};
};
