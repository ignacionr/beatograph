#pragma once

#include <string>

#include <nlohmann/json.hpp>
#include <imgui.h>
#include "../views/json.hpp"
#include "../views/cached_view.hpp"
#include "host.hpp"
#include "user_screen.hpp"
#include "issue_screen.hpp"

namespace jira
{
    struct project_screen
    {
        project_screen()
        {
        }

        void render(nlohmann::json::object_t const &json_project, host& host)
        {
            auto const key {json_project.at("key").get<std::string>()};
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
            ImGui::Text("%s", json_project.at("name").get<std::string>().c_str());
            ImGui::PopFont();
            ImGui::Text("Key: %s", key.c_str());
            if (json_project.contains("lead"))
            {
                ImGui::SeparatorText("Lead");
                user_screen_.render(json_project.at("lead"));
            }
            views::cached_view<nlohmann::json::array_t>("To Do",
                [key, &host]() {
                    return nlohmann::json::parse(host.search_by_project(key)).at("issues");
                },
                [this, &host](nlohmann::json::array_t const &json_components) {
                    if (json_components.empty())
                    {
                        ImGui::Text("No To Do found.");
                    }
                    else
                    {
                        for (auto const &issue : json_components)
                        {
                            issue_screen_.render(issue, host);
                        }
                    }
                });
        }

    private:
        views::json json;
        user_screen user_screen_;
        issue_screen issue_screen_;
    };
}