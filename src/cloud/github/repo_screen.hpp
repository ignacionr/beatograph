#pragma once

#include <memory>
#include <string>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "../../structural/views/cached_view.hpp"
#include "../../structural/views/json.hpp"
#include "host.hpp"
#include "../../registrar.hpp"
#include "../../imgcache.hpp"

namespace github::repo {
    struct screen {
        screen(nlohmann::json::object_t &&repo, std::shared_ptr<host> host): repo_{repo}, host_{host} {}

        std::string const &name() const {
            return repo_.at("name").get_ref<const std::string&>();
        }

        void render() {
            auto const& repo_name {repo_.at("name").get_ref<const std::string&>()};
            ImGui::PushID(repo_name.c_str());
            if (ImGui::BeginTable(repo_name.c_str(), 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
                static views::json json_view;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(repo_name.data(), repo_name.data() + repo_name.size());
                if (ImGui::SameLine(); ImGui::SmallButton("Details")) {
                    show_details_ = !show_details_;
                }
                if (show_details_) {
                    json_view.render(repo_);
                }
                ImGui::TableNextColumn();
                views::cached_view<nlohmann::json>("Workflows",
                    [this] { return host_->repo_workflows(repo_.at("full_name").get_ref<const std::string&>()); },
                    [this](nlohmann::json const &workflows) {
                        if (show_details_) {
                            json_view.render(workflows);
                        }
                        for (auto const &workflow : workflows.at("workflows")) {
                            // is there a badge_url?
                            if (workflow.contains("badge_url")) {
                                auto cache = registrar::get<img_cache>({});
                                auto const badge_url = workflow.at("badge_url").get_ref<const std::string&>();
                                auto const texture_id = cache->load_texture_from_url(badge_url, host_->header_client());
                                if (texture_id != cache->default_texture()) {
                                    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<long long>(texture_id)), ImVec2(170, 17));
                                }
                            }
                            else {
                                ImGui::TextUnformatted(workflow.at("name").get_ref<const std::string&>().c_str());
                            }
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
        bool show_details_{false};
    };
}
