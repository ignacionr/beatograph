#pragma once

#include <string>
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

        void render(nlohmann::json const &json, host& h, bool expanded = false)
        {
            auto const key{json["key"].get<std::string>()};
            ImGui::PushID(key.c_str());
            std::string color_name {"unknown"};
            nlohmann::json::json_pointer ptr {"/fields/status/statusCategory/colorName"};
            if (json.contains(ptr)) {
                color_name = json.at("fields").at("status").at("statusCategory").at("colorName").get<std::string>();
            }
            ImGui::PushStyleColor(ImGuiCol_Header, color(color_name));
            if (ImGui::CollapsingHeader(key.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
                nlohmann::json::object_t const &fields {json.at("fields").get<nlohmann::json::object_t>()};
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {20, 20});
                ImGui::PushStyleColor(ImGuiCol_FrameBg, color(color_name));
                ImGui::BeginChild(std::format("summary-{}", key).c_str(), {ImGui::GetColumnWidth(), 0}, ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY);
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGui::TextWrapped("%s\n \n", fields.at("summary").get<std::string>().c_str());
                ImGui::PopFont();
                ImGui::SeparatorText("Asignee");
                if (fields.find("assignee") != fields.end() && fields.at("assignee").is_object()) {
                    user_screen_.render(fields.at("assignee"));
                }
                else {
                    ImGui::Text("Unassigned");
                }
                if (ImGui::Button("Assign to me")) {
                    h.assign_issue_to_me(key);
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
                        render(subtask, h);
                    }
                    ImGui::Unindent();
                }
                ImGui::Indent();
                if (ImGui::CollapsingHeader("All Details"))
                {
                    json_view_.render(fields);
                }
                ImGui::Unindent();
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }

    private:
        views::json json_view_;
        user_screen user_screen_;
    };
} // namespace jira
