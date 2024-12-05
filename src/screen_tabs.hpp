#pragma once
#include <algorithm>
#include <functional>
#include <string>
#include <tuple>
#include <vector>

#include <imgui.h>

#include "group_t.hpp"

struct screen_tabs {
    using tab_changed_sink_t = std::function<void(std::string_view)>;
    screen_tabs(std::vector<group_t> tabs, tab_changed_sink_t tab_changed) 
    : tabs{std::move(tabs)}, tab_changed_{tab_changed} {}

    void render()
    {
        if (ImGui::BeginTabBar("Tabs")) {
            ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.05f, 0.05f, 0.05f, 0.8f));
            current_menu = {};
            for (const auto &tab : tabs)
            {
                ImGui::PushStyleColor(ImGuiCol_TabActive, tab.color);
                ImGui::PushStyleColor(ImGuiCol_TabHovered, tab.color);
                ImGuiTabItemFlags flags = 0;
                if (select_tab_ == tab.name) {
                    flags |= ImGuiTabItemFlags_SetSelected;
                    select_tab_.clear();
                }
                if (ImGui::BeginTabItem(tab.name.c_str(), nullptr, flags))
                {
                    if (tab.name != current_tab_) {
                        current_tab_ = tab.name;
                        tab_changed_(current_tab_);
                    }
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

    void select(std::string_view name) {
        select_tab_ = name;
        tab_changed_(name);
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
    std::string select_tab_;
    std::string current_tab_;
    tab_changed_sink_t tab_changed_;
};