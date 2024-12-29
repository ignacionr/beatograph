#pragma once

#include <functional>

#include <imgui.h>

struct split_screen {
    using render_t = std::function<void()>;
    using menu_t = std::function<void(std::string_view)>;

    split_screen(render_t left, render_t right, menu_t left_menu = {}, menu_t right_menu = {}) : 
    left_{left}, right_{right}, left_menu_{left_menu}, right_menu_{right_menu} {}

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
        if (left_menu_) {
            left_menu_(item);
        }
        if (right_menu_) {
            right_menu_(item);
        }
    }
private:
    render_t left_;
    render_t right_;
    menu_t left_menu_{};
    menu_t right_menu_{};
};
