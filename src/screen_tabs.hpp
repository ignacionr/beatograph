#pragma once
#include <functional>
#include <string>
#include <imgui.h>
#include <vector>
#include <utility>

struct screen_tabs {
    using tab_t = std::pair<std::string, std::function<void()>>;
    screen_tabs(std::vector<tab_t> tabs) : tabs{std::move(tabs)} {}
    void render()
    {
        ImGui::BeginTabBar("Tabs");
        for (const auto &[name, render] : tabs)
        {
            if (ImGui::BeginTabItem(name.c_str()))
            {
                render();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
private:
    std::vector<tab_t> tabs;
};