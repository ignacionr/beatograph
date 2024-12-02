#pragma once

#include <functional>

#include <imgui.h>

struct split_screen {
    using render_t = std::function<void()>;
    split_screen(render_t left, render_t right) : left_{left}, right_{right} {}
    void render() {
        if (ImGui::BeginChild("Left", ImVec2{ImGui::GetWindowWidth() * 3 / 4 - 20, 0})) {
            left_();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if (ImGui::BeginChild("Right", ImVec2{ImGui::GetWindowWidth() * 1 / 4, 0})) {
            right_();
        }
        ImGui::EndChild();
    }
    void render_menu(std::string_view item) {
        if (current_menu) {
            current_menu(item);
        }
    }
private:
    render_t left_;
    render_t right_;
    std::function<void(std::string_view)> current_menu{};
};
