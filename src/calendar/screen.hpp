#pragma once

#include <array>
#include <string>

struct calendar_screen {
    void render() {
        static constexpr std::array<std::string_view, 7> days = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
        if (ImGui::BeginTable("Calendar", 7, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
            for (auto const day: days) {
                ImGui::TableSetupColumn(day.data());
            }
            ImGui::TableHeadersRow();
            // determine what day of the week the first day of the month is
            auto const now = std::chrono::system_clock::now();
            auto const now_time = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_s(&tm, &now_time);
            tm.tm_mday = 1;
            std::mktime(&tm);
            auto const first_day_of_month = tm.tm_wday;
            for (int i = 0; i < first_day_of_month; ++i) {
                ImGui::TableNextColumn();
            }
            // determine how many days are in the month
            tm.tm_mday = 32;
            std::mktime(&tm);
            auto const days_in_month = 32 - tm.tm_mday;
            for (int i = 1; i <= days_in_month; ++i) {
                ImGui::TableNextColumn();
                ImGui::Text("%d", i);
            }
            ImGui::EndTable();
        }
    }
};