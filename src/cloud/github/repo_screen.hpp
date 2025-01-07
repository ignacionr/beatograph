#pragma once

#include <memory>
#include <string>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "../../imgcache.hpp"
#include "../../registrar.hpp"
#include "../../structural/views/cached_view.hpp"
#include "../../structural/views/json.hpp"
#include "host.hpp"
#include "../../hosting/host_local.hpp"

namespace github::repo
{
    struct screen
    {
        screen(nlohmann::json::object_t &&repo, std::shared_ptr<host> host) : repo_{repo}, host_{host} {}

        std::string const &name() const
        {
            return repo_.at("name").get_ref<const std::string &>();
        }

        std::string const &full_name() const
        {
            return repo_.at("full_name").get_ref<const std::string &>();
        }

        void render()
        {
            auto const &repo_name{name()};
            ImGui::PushID(repo_name.c_str());
            if (ImGui::BeginTable(repo_name.c_str(), 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg))
            {
                static views::json json_view;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(repo_name.data(), repo_name.data() + repo_name.size());
                if (ImGui::SameLine(); ImGui::SmallButton("Details"))
                {
                    show_details_ = !show_details_;
                }
                if (show_details_)
                {
                    json_view.render(repo_);
                }
                ImGui::TableNextColumn();
                views::cached_view<nlohmann::json>("Workflows", [this]
                                                   { return host_->repo_workflows(full_name()); }, [this](nlohmann::json const &workflows)
                                                   {
                        if (show_details_) {
                            json_view.render(workflows);
                        }
                        for (auto const &workflow : workflows.at("workflows")) {
                            ImGui::PushID(workflow.at("name").get_ref<const std::string&>().c_str());
                            // is there a badge_url?
                            bool badge_painted{false};
                            if (workflow.contains("badge_url")) {
                                auto cache = registrar::get<img_cache>({});
                                auto const badge_url = workflow.at("badge_url").get_ref<const std::string&>();
                                auto const texture_id = cache->load_texture_from_url(badge_url, host_->header_client());
                                if (texture_id != cache->default_texture()) {
                                    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<long long>(texture_id)), ImVec2(170, 17));
                                    badge_painted = true;
                                }
                            }
                            if (!badge_painted) {
                                ImGui::TextUnformatted(workflow.at("name").get_ref<const std::string&>().c_str());
                            }
                            views::cached_view<nlohmann::json>("Runs", [this, &workflow]
                                                            { 
                                                                return host_->fetch(workflow.at("url").get<std::string>() + "/runs");
                                                            },
                                                             [this](nlohmann::json const &runs)
                                                             {
                                if (show_details_) {
                                    json_view.render(runs);
                                }
                                if (ImGui::BeginTable("runs", 5, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg))
                                {
                                    ImGui::TableSetupColumn("Status");
                                    ImGui::TableSetupColumn("Title");
                                    ImGui::TableSetupColumn("Started At");
                                    ImGui::TableSetupColumn("Hash");
                                    ImGui::TableSetupColumn("##actions");
                                    ImGui::TableHeadersRow();
                                    for (auto const &run : runs.at("workflow_runs")) {
                                        ImGui::PushID(run.at("id").get<int>());
                                        ImGui::TableNextRow();
                                        ImGui::TableNextColumn();
                                        auto const &conclusion {run.at("conclusion").get_ref<const std::string&>()};
                                        bool const is_ok = conclusion == "success";
                                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(is_ok ? ImVec4{0.0f, 1.0f, 0.0f, 0.5f} : ImVec4{1.0f, 0.0f, 0.0f, 0.5f}));
                                        ImGui::TextUnformatted(is_ok ? ICON_MD_CHECK_CIRCLE : ICON_MD_CANCEL);
                                        ImGui::SameLine();
                                        ImGui::TextUnformatted(run.at("status").get_ref<const std::string&>().c_str());
                                        ImGui::SameLine();
                                        ImGui::TextUnformatted(conclusion.c_str());
                                        ImGui::TableNextColumn();
                                        ImGui::TextUnformatted(run.at("display_title").get_ref<const std::string&>().c_str());
                                        ImGui::TableNextColumn();
                                        ImGui::TextUnformatted(run.at("run_started_at").get_ref<const std::string&>().c_str());
                                        ImGui::TableNextColumn();
                                        ImGui::TextUnformatted(run.at("head_sha").get_ref<const std::string&>().c_str());
                                        ImGui::TableNextColumn();
                                        if (ImGui::SmallButton(ICON_MD_WEB " Open...")) {
                                            auto local_host = registrar::get<hosting::local::host>({});
                                            local_host->open_content(run.at("html_url").get<std::string>());
                                        }
                                        ImGui::PopID();
                                    }
                                }
                                ImGui::EndTable();
                            });
                            ImGui::PopID();
                        } });
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
