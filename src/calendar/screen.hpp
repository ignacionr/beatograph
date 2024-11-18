#pragma once

#include <array>
#include <chrono>
#include <ctime>
#include <string>

struct calendar_screen {
    void render() {
        static constexpr std::array<std::string_view, 7> days = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
        static constexpr std::array<std::string_view, 12> months = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
        // determine what day of the week the first day of the month is
        auto const now = std::chrono::system_clock::now();
        auto const now_time = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &now_time);
        auto const current_month = tm.tm_mon;
        auto const current_year = 1900 + tm.tm_year;
        auto const current_day_of_month = tm.tm_mday;
        tm.tm_mday = 1;
        std::mktime(&tm);
        auto const first_day_of_month = tm.tm_wday;

        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
        auto const month_title {std::format("{} {}", months[current_month], current_year)};
        ImGui::TextUnformatted(month_title.c_str());
        ImGui::PopFont();

        if (ImGui::BeginTable("Month-View", 7, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
            for (auto const day: days) {
                ImGui::TableSetupColumn(day.data());
            }
            ImGui::TableHeadersRow();
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
                if (i == current_day_of_month) {
                    ImGui::TextUnformatted("Today");
                }
            }
            ImGui::EndTable();
        }
    }
};