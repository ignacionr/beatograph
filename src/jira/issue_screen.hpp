#pragma once

#include <string>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <imgui.h>
#include "../views/json.hpp"
#include "colors.hpp"
#include "user_screen.hpp"
#include "../imgcache.hpp"

namespace jira
{
    struct issue_screen
    {
        issue_screen(img_cache &cache): user_screen_{cache} {
        }

        void do_async(std::function<void()> fn) {
            // std::thread([fn]{
                try {
                    fn();
                }
                catch(std::exception const &e) {
                    std::cerr << "Error in async function: " << e.what() << "\n";
                }
                catch(...) {
                    std::cerr << "Error in async function\n";
                }
            //}).detach();
        }

        bool render(nlohmann::json const &json, host& h, bool expanded = false, bool show_json_details = false, bool show_assignee = false)
        {
            bool request_requery = false;
            auto const key{json.at("key").get<std::string>()};
            ImGui::PushID(key.c_str());
            std::string color_name {"unknown"};
            static const nlohmann::json::json_pointer status_color_ptr {"/fields/status/statusCategory/colorName"};
            if (json.contains(status_color_ptr)) {
                color_name = json.at(status_color_ptr).get<std::string>();
            }
            ImGui::PushStyleColor(ImGuiCol_Header, color(color_name));
            static const nlohmann::json::json_pointer issue_type_name_ptr {"/fields/issuetype/name"};
            if (ImGui::CollapsingHeader(std::format("{} - {}", key, json.at(issue_type_name_ptr).get<std::string>()).c_str(),
                expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) 
            {
                nlohmann::json::object_t const &fields {json.at("fields").get<nlohmann::json::object_t>()};
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {20, 20});
                ImGui::PushStyleColor(ImGuiCol_FrameBg, color(color_name));
                ImGui::BeginChild(std::format("summary-{}", key).c_str(), {ImGui::GetColumnWidth(), 0}, ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY);
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGui::TextWrapped("%s\n \n", fields.at("summary").get<std::string>().c_str());
                ImGui::PopFont();
                if (show_assignee) {
                    ImGui::SeparatorText("Asignee");
                    if (fields.find("assignee") != fields.end() && fields.at("assignee").is_object()) {
                        user_screen_.render(fields.at("assignee"));
                    }
                    else {
                        ImGui::Text("Unassigned");
                    }
                }
                if (ImGui::SmallButton("Assign to me")) {
                    do_async([&h, key] { h.assign_issue_to_me(key); });
                    request_requery = true;
                }
                if (ImGui::SameLine(); ImGui::SmallButton("Unassign")) {
                    do_async([&h, key] { h.unassign_issue(key); });
                    request_requery = true;
                }
                if (ImGui::SameLine(); ImGui::SmallButton("Mark as Done")) {
                    do_async([&h, key]{ h.transition_issue(key, "31");});
                    request_requery = true;
                }
                if (ImGui::SameLine(); ImGui::SmallButton("Mark In Progress")) {
                    do_async([&h, key]{ h.transition_issue(key, "21");});
                    request_requery = true;
                }
                if (ImGui::SameLine(); ImGui::SmallButton("Open Web...")) {
                    auto const url = std::format("https://betmavrik.atlassian.net/browse/{}", key);
                    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
                // are there subtasks?
                if (fields.find("subtasks") != fields.end())
                {
                    ImGui::Indent();
                    nlohmann::json::array_t const &subtasks {fields.at("subtasks").get<nlohmann::json::array_t>()};
                    for (nlohmann::json const &subtask : subtasks)
                    {
                        request_requery |= render(subtask, h, false, show_json_details, show_assignee);
                    }
                    ImGui::Unindent();
                }
                if (show_json_details) {
                    ImGui::Indent();
                    if (ImGui::CollapsingHeader("All Details"))
                    {
                        json_view_.render(fields);
                    }
                    ImGui::Unindent();
                }
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
            return request_requery;
        }

    private:
        views::json json_view_;
        user_screen user_screen_;
    };
} // namespace jira
