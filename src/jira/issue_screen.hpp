#pragma once

#include <nlohmann/json.hpp>
#include <imgui.h>
#include "../views/json.hpp"

namespace jira
{
    struct issue_screen
    {
        void render(nlohmann::json const &json)
        {
            auto const key{json["key"].get<std::string>()};
            if (ImGui::CollapsingHeader(key.c_str())) {
                nlohmann::json::object_t const &fields {json.at("fields").get<nlohmann::json::object_t>()};
                ImGui::TextWrapped("%s", fields.at("summary").get<std::string>().c_str());
                // are there subtasks?
                if (fields.find("subtasks") != fields.end())
                {
                    ImGui::Indent();
                    nlohmann::json::array_t const &subtasks {fields.at("subtasks").get<nlohmann::json::array_t>()};
                    for (nlohmann::json const &subtask : subtasks)
                    {
                        render(subtask);
                    }
                    ImGui::Unindent();
                }
                ImGui::Indent();
                if (ImGui::CollapsingHeader("All Details"))
                {
                    json_view.render(fields);
                }
                ImGui::Unindent();
            }
        }

    private:
        views::json json_view;
    };
} // namespace jira
