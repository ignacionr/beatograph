#pragma once

#include <string>
#include <imgui.h>

#include "host.hpp"

namespace structural::text_command {
    struct screen {
        screen(host &host) : host_{host} {}

        void render() {
            ImGui::PushID("text_command");
            if (search_text_.reserve(256); ImGui::InputText("##text_command", search_text_.data(), search_text_.capacity())) {
                search_text_ = search_text_.data();
            }
            if (ImGui::IsItemDeactivatedAfterEdit() && selected_index_ == -1) {
                try {
                    host_(search_text_);
                    search_text_.clear();
                }
                catch (std::exception const &e) {
                    search_text_ = e.what();
                }
            }
            ImGui::PopID();
            bool const focused{ImGui::IsItemActive()};
            if (focused) {
                if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow))) {
                    ++selected_index_;
                }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow))) {
                    --selected_index_;
                }
            }
            if (ImGui::BeginListBox("##text_command")) {
                if (-1 != selected_index_) {
                    ImGui::SetScrollY(selected_index_ * ImGui::GetTextLineHeight());
                }
                int item_index{-1};
                host_.partial_list(search_text_, [this, focused, &item_index](std::string const &name) {
                    bool const is_selected{++item_index == selected_index_};
                    if (ImGui::Selectable(name.c_str(), is_selected) || (is_selected && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)))) {
                        result_ = host_(name);
                        selected_index_ = -1;
                        search_text_.clear();
                    }
                });
                ImGui::EndListBox();
                if (!result_.empty()) {
                    ImGui::TextWrapped("%s", result_.c_str());
                }
            }
        }
    private:
        host &host_;
        std::string search_text_;
        std::string result_;
        int selected_index_{-1};
    };
}