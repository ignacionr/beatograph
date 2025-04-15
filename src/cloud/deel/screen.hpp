#pragma once

#include <functional>
#include <string>

#include <imgui.h>

#include "../../structural/views/cached_view.hpp"
#include "data.hpp"
#include "host.hpp"
#include "../../registrar.hpp"

namespace cloud::deel
{
    struct screen {

        static void RightAlignedText(const char* text, float width) {
            ImVec2 textSize = ImGui::CalcTextSize(text);
            float posX = ImGui::GetCursorPosX() + width - textSize.x;
            if (posX > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(posX);
            ImGui::Text("%s", text);
        }

        void render(auto &host_ptr) const noexcept {
            views::cached_view<history_res>(
                "History",
                [&host_ptr]() -> history_res {
                    return host_ptr->fetch_history();
                },
                [](history_res const &history) {
                    if (!history.has_value()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", history.error().c_str());
                        return;
                    }
                    if (history->rows.empty()) {
                        ImGui::Text("No history found.");
                        return;
                    }
                    if (ImGui::BeginTable("Deel History", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                        ImGui::TableSetupColumn("Organization", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                        ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                        ImGui::TableHeadersRow();
                        for (auto const &row : history->rows) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", row.label.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", row.type.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", row.organization.name.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", row.description.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", row.status.c_str());
                            ImGui::TableNextColumn();
                            RightAlignedText(row.total.c_str(), 150.0f);
                        }
                        ImGui::EndTable();
                    }
                });
            if (ImGui::SmallButton("Open in Browser")) {
                auto const openfn = registrar::get<std::function<void(std::string const &)>>("open");
                (*openfn)("https://app.deel.com/");
            }
        }
    };
} // namespace cloud::deel
