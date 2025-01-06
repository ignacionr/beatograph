#pragma once

#include <memory>
#include <string>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "../../structural/views/cached_view.hpp"
#include "../../structural/views/json.hpp"
#include "host.hpp"
#include "../../registrar.hpp"

namespace github::repo {
    struct screen {
        screen(nlohmann::json::object_t &&repo, std::shared_ptr<host> host): repo_{repo}, host_{host} {}
        void render() {
            auto const& repo_name {repo_.at("name").get_ref<const std::string&>()};
            ImGui::PushID(repo_name.c_str());
            if (ImGui::BeginTable(repo_name.c_str(), 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(repo_name.data(), repo_name.data() + repo_name.size());
                ImGui::TableNextColumn();
                views::cached_view<nlohmann::json>("Workflows",
                    [this] { return host_->repo_workflows(repo_.at("full_name").get_ref<const std::string&>()); },
                    [](nlohmann::json const &workflows) {
                        static views::json json_view;
                        json_view.render(workflows);
                        for (auto const &workflow : workflows.at("workflows")) {
                            ImGui::TextUnformatted(workflow.at("name").get_ref<const std::string&>().c_str());
                        }
                    }
                );
            }
            ImGui::EndTable();
            ImGui::PopID();
        }   
    private:
        nlohmann::json::object_t repo_;
        std::shared_ptr<host> host_;
    };
}
