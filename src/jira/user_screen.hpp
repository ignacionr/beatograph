#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace jira
{
    struct user_screen
    {
        void render(nlohmann::json const &json_user) {
            ImGui::Text("%s", json_user.at("displayName").get<std::string>().c_str());
            ImGui::Text("%s", json_user.at("emailAddress").get<std::string>().c_str());
            if (!json_user["active"].get<bool>()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "(Inactive)");
            }
            ImGui::Text("%s", json_user.at("timeZone").get<std::string>().c_str());
        }
    };
} // namespace jira
