#pragma once

#include <algorithm>
#include <atomic>
#include <memory>
#include <ranges>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include "../host/host.hpp"
#include "../host/screen.hpp"
#include "../docker/host.hpp"

struct importer_report {
    importer_report(host_local &localhost): 
        host_importer_{"ignacio-bench"}, 
        docker_host_{host_importer_},
        localhost_{localhost} 
    {
        auto t = std::thread([this] {
            host_importer_.resolve_from_ssh_conf(localhost_);
            try {
                host_importer_.fetch_metrics(localhost_);
                docker_ps_.store(std::make_shared<nlohmann::json>(std::move(docker_host_.ps(localhost_))));
            }
            catch(std::exception const &e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        });
        t.detach();
    }
    void render() 
    {
        if (ImGui::CollapsingHeader("Importer Report", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Our importer runs on a docker container");
            if (ImGui::CollapsingHeader("Importer Docker Host Status")) {
                host_screen_.render(host_importer_);
                ImGui::Indent();
                if (ImGui::CollapsingHeader("Containers")) {
                    auto ps = docker_ps_.load();
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
                            auto t = std::thread([this] {
                                try {
                                    docker_ps_.store(std::make_shared<nlohmann::json>(std::move(docker_host_.ps(localhost_))));
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
                ImGui::Unindent();
            }
        }
    }
private:
    host host_importer_;
    docker_host docker_host_;
    host_screen host_screen_;
    host_local &localhost_;
    std::atomic<std::shared_ptr<nlohmann::json>> docker_ps_;
};
