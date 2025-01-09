#pragma once

#include <memory>
#include <string>
#include <stdexcept>

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cppgpt/cppgpt.hpp>

#include "../../imgcache.hpp"
#include "../../registrar.hpp"
#include "../../structural/views/cached_view.hpp"
#include "../../structural/views/json.hpp"
#include "host.hpp"
#include "../../hosting/host_local.hpp"
#include "../../util/conversions/base64.hpp"

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
            if (ImGui::BeginTable(repo_name.c_str(), 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg))
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
                                        auto const &conclusion_el {run.at("conclusion")};
                                        auto const &conclusion {conclusion_el.is_string() ? conclusion_el.get_ref<const std::string&>() : "pending"};
                                        bool const is_ok = conclusion == "success";
                                        if (conclusion == "failure") {
                                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImVec4{1.0f, 0.0f, 0.0f, 0.5f}));
                                            ImGui::TextUnformatted(ICON_MD_CANCEL);
                                        }
                                        else if (conclusion == "success") {
                                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImVec4{0.0f, 1.0f, 0.0f, 0.5f}));
                                            ImGui::TextUnformatted(ICON_MD_CHECK_CIRCLE);
                                        }
                                        else {
                                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImVec4{0.0f, 0.0f, 1.0f, 0.25f}));
                                            ImGui::TextUnformatted(ICON_MD_RUN_CIRCLE);
                                        }
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
                                        if (ImGui::SameLine(); ImGui::SmallButton(ICON_MD_CHAT_BUBBLE " Explain...")) {
                                            try {
                                                auto gpt = registrar::get<ignacionr::cppgpt>({});
                                                gpt->clear();
                                                gpt->add_instructions("You are a GitHub Actions workflow run. What can you tell me about yourself as per the following?");
                                                // try to get the logs
                                                auto const &logs_url {run.at("logs_url").get_ref<const std::string&>()};
                                                try {
                                                    auto raw_logs = host_->fetch_string(logs_url);
                                                    auto base_64_logs = conversions::text_to_base64(raw_logs);
                                                    gpt->add_instructions("Here are your logs in base64 encoding; read them and understand them to provide an accurate description:");
                                                    if (base_64_logs.size() > 5000) {
                                                        // keep the last 5k characters
                                                        base_64_logs = base_64_logs.substr(base_64_logs.size() - 5000);
                                                    }
                                                    gpt->add_instructions(base_64_logs);
                                                    gpt->sendMessage(is_ok ?  "Summarize this run and its outcome; if there are assets produced/released (look in the log contents), explain how to access them." : "Using the log contents, explain the failure and how to proceed.", "user", "llama3-70b-8192");
                                                }
                                                catch(std::exception &) {
                                                    gpt->sendMessage("Explain this run to me. The logs aren't available.", "user", "llama-3.3-70b-specdec");
                                                }
                                            }
                                            catch(std::exception &e) {
                                                // notify the error
                                                auto notify = registrar::get<std::function<void(std::string_view)>>({"notify"});
                                                (*notify)(e.what());
                                            }
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
