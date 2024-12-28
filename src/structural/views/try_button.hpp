#pragma once

#include <string>
#include <functional>
#include <unordered_map>

#include <imgui.h>

namespace views {
    void try_button(std::string_view caption, std::function<void()> action) {
        static std::unordered_map<ImGuiID, std::string> errors_;
        if (auto pos = errors_.find(ImGui::GetID(caption.data())); pos != errors_.end()) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", pos->second.c_str());
        }
        if (ImGui::SmallButton(caption.data())) {
            try {
                action();
                errors_.erase(ImGui::GetID(caption.data()));
            }
            catch (std::exception const &e) {
                errors_[ImGui::GetID(caption.data())] = e.what();
            }
            catch (...) {
                errors_[ImGui::GetID(caption.data())] = "Unknown error";
            }
        }
    }
}