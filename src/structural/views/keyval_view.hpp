#pragma once

#include <string>

#include <imgui.h>

namespace views {
    void keyval(auto const &dict, const char *k = "Key", const char *v = "Value") noexcept {
        if (ImGui::BeginTable("##keyval", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn(k, ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableSetupColumn(v, ImGuiTableColumnFlags_WidthStretch, 0.75f);
            for (auto const &[key, value] : dict) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(key.data(), key.data() + key.size());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.data(), value.data() + value.size());
            }
            ImGui::EndTable();
        }
    }
}