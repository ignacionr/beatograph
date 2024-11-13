#pragma once
#include <functional>
#include <string>
#include <imgui.h>
#include <vector>
#include <tuple>

struct screen_tabs {

    struct tab_t {
        std::string name;
        std::function<void()> render;
        std::function<void(std::string_view)> render_menu = {};
        ImVec4 color = ImVec4(0.1f, 0.1f, 0.5f, 1.0f);
    };
    screen_tabs(std::vector<tab_t> tabs) : tabs{std::move(tabs)} {}
    void render()
    {
        if (ImGui::BeginTabBar("Tabs")) {
            ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            current_menu = {};
            for (const auto &tab : tabs)
            {
                ImGui::PushStyleColor(ImGuiCol_TabActive, tab.color);
                ImGui::PushStyleColor(ImGuiCol_TabHovered, tab.color);
                if (ImGui::BeginTabItem(tab.name.c_str()))
                {
                    current_menu = tab.render_menu;
                    tab.render();
                    ImGui::EndTabItem();
                }
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();
            }
            ImGui::PopStyleColor();
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