#pragma once

#include <string>

#include <imgui.h>
#include "group_t.hpp"

struct tool_screen {
    tool_screen(std::vector<group_t> &&tabs) : tabs{std::move(tabs)} {}

    void render()
    {
        for (const auto &tab : tabs)
        {
            if (ImGui::CollapsingHeader(tab.name.c_str()))
            {
                tab.render();
            }
        }
    }

    void render_menu(std::string_view item) {
        if (current_menu) {
            current_menu(item);
        }
    }

    void select(std::string_view name) {
        select_tab_ = name;
    }

    size_t add(group_t tab) {
        tabs.push_back(tab);
        return tabs.size() - 1;
    }

    void remove(std::string_view name) {
        tabs.erase(std::remove_if(tabs.begin(), tabs.end(), [name](const group_t &tab) {
            return tab.name == name;
        }), tabs.end());
    }
private:
    std::vector<group_t> tabs;
    std::function<void(std::string_view)> current_menu{};
    std::string select_tab_{};
};