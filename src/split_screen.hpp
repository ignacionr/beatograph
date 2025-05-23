#pragma once

#include <functional>
#include <string>

#include <imgui.h>

struct split_screen 
{
    using render_t = std::function<void()>;
    using menu_t = std::function<void(std::string_view)>;

    split_screen(render_t left, render_t right, menu_t left_menu = {}, menu_t right_menu = {}) : 
    left_{left}, right_{right}, left_menu_{left_menu}, right_menu_{right_menu} {}

    void render() noexcept {
        if (show_left_) {
            bool full_width = !show_right_;
            if (ImGui::BeginChild("Left",
                ImVec2{
                    full_width ? ImGui::GetWindowWidth() - 13
                        : ImGui::GetWindowWidth() * 3 / 4 - 20
                    , 0}
            )) {
                left_();
            }
            ImGui::EndChild();
            ImGui::SameLine();
        }
        if (show_right_) {
            if (ImGui::BeginChild("Right", ImVec2{ImGui::GetWindowWidth() * 1 / 4, 0})) {
                right_();
            }
            ImGui::EndChild();
        }
        else if (show_left_) {
            ImGui::NewLine();
        }
    }

    void render_menu(std::string_view item) {
        if (left_menu_) {
            left_menu_(item);
        }
        if (right_menu_) {
            right_menu_(item);
        }
    }

    bool &show_left() {
        return show_left_;
    }
    bool &show_right() {
        return show_right_;
    }

private:
    render_t left_;
    render_t right_;
    menu_t left_menu_{};
    menu_t right_menu_{};
    bool show_left_{true};
    bool show_right_{true};
};
