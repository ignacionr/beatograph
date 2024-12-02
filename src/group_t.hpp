#pragma once

#include <functional>
#include <string>

#include <imgui.h>

struct group_t {
    std::string name;
    std::function<void()> render;
    std::function<void(std::string_view)> render_menu = {};
    ImVec4 color = ImVec4(0.1f, 0.1f, 0.5f, 1.0f);
};
