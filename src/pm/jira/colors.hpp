#pragma once

#include <string>
#include <unordered_map>
#include <imgui.h>

namespace jira
{
    static ImVec4 color(std::string_view color_name) {
        static std::unordered_map<std::string_view, ImVec4> const known_colors {
            {"medium-gray", {0.5f, 0.5f, 0.5f, 1.0f}},
            {"green", {0.0f, 0.66f, 0.0f, 1.0f}},
            {"blue-gray", {0.0f, 0.0f, 1.0f, 1.0f}},
            {"yellow", {0.5f, 0.5f, 0.0f, 1.0f}},
            {"red", {1.0f, 0.0f, 0.0f, 1.0f}},
            {"blue", {0.0f, 1.0f, 1.0f, 1.0f}},
            {"purple", {1.0f, 0.0f, 1.0f, 1.0f}},
            {"gray", {0.5f, 0.5f, 0.5f, 1.0f}},
            {"light-gray", {0.8f, 0.8f, 0.8f, 1.0f}},
            {"dark-gray", {0.2f, 0.2f, 0.2f, 1.0f}},
            {"orange", {1.0f, 0.5f, 0.0f, 1.0f}},
            {"brown", {0.5f, 0.25f, 0.0f, 1.0f}},
            {"teal", {0.0f, 0.5f, 0.5f, 1.0f}},
            {"light-blue", {0.0f, 0.5f, 1.0f, 1.0f}},
            {"light-green", {0.5f, 1.0f, 0.5f, 1.0f}},
            {"dark-green", {0.0f, 0.5f, 0.0f, 1.0f}},
            {"dark-blue", {0.0f, 0.0f, 0.5f, 1.0f}}};
        auto const it = known_colors.find(color_name);
        return it != known_colors.end() ? it->second : ImVec4{0.8f, 0.8f, 0.8f, 1.0f};
    }
} // namespace jira
