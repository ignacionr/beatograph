#pragma once

#include <functional>

#include <imgui.h>

struct split_window {
    using render_t = std::function<void()>;
    split_window(render_t left, render_t right) : left_{left}, right_{right} {}
    void render() {
        if (ImGui::BeginChild("Split", ImVec2{0, 0}, true)) {
            ImGui::BeginChild("Left", ImVec2{ImGui::GetWindowWidth() * 3 / 4, 0});
            left_();
            ImGui::EndChild();
            ImGui::BeginChild("Right", ImVec2{0, 0});
            right_();
            ImGui::EndChild();
        }
    }
private:
    render_t left_;
    render_t right_;
};
