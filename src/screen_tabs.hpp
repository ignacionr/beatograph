#pragma once
#include <functional>
#include <string>
#include <imgui.h>
#include <vector>
#include <tuple>

struct screen_tabs {
    using tab_t = std::tuple<std::string, std::function<void()>, std::function<void(std::string_view)>>;
    screen_tabs(std::vector<tab_t> tabs) : tabs{std::move(tabs)} {}
    void render()
    {
        if (ImGui::BeginTabBar("Tabs")) {
            current_menu = {};
            for (const auto &[name, render, render_menu] : tabs)
            {
                if (ImGui::BeginTabItem(name.c_str()))
                {
                    current_menu = render_menu;
                    render();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    void render_menu(std::string_view item) {
        if (current_menu) {
            current_menu(item);
        }
    }
private:
    std::vector<tab_t> tabs;
    std::function<void(std::string_view)> current_menu{};
};