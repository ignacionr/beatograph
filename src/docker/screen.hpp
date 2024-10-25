#include "host.hpp"
#include "../host/host.hpp"
#include "../host/host_local.hpp"
#include <imgui.h>

struct docker_screen {
    void render(docker_host<host> &host, host_local &localhost) {
        if (ImGui::CollapsingHeader("Containers")) {
            auto const &ps = host.ps();
            if (ps) {
                auto const &array {ps->get<nlohmann::json::array_t>()};
                if (array.empty()) {
                    ImGui::Text("No containers.");
                }
                else {
                    // get the columns from the first container
                    auto cols = array.front().items() | 
                        std::views::transform([](auto const &item) { return item.key(); });
                    ImGui::BeginTable("Docker Containers", 
                        static_cast<int>(std::ranges::distance(cols)), 
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | 
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable);
                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                    for(auto const &column_name: cols) {
                        ImGui::Text("%s", column_name.c_str());
                        ImGui::TableSetColumnIndex(ImGui::TableGetColumnIndex() + 1);
                    }
                    for (auto const &container : array) {
                        ImGui::TableNextRow();
                        for (auto const&[key, value]: container.items()) {
                            ImGui::Text("%s", value.dump().c_str());
                            ImGui::TableSetColumnIndex(ImGui::TableGetColumnIndex() + 1);
                        }
                    }
                    ImGui::EndTable();
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
            else {
                ImGui::Text("N/A");
            }
        }
    }
};
