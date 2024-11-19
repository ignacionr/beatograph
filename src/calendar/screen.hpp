#pragma once

#include <array>
#include <chrono>
#include <ctime>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "host.hpp"
#include "../views/cached_view.hpp"

namespace calendar
{
    struct screen
    {
        screen(host &h) : host_(h) {}

        void render() {
            views::cached_view<event_set>("calendar", 
                [this] { return host_.get_events(); }, 
                [this](auto const &events) { render_calendar(events); }, true);
        }

        void render_calendar(auto const &events)
        {
            static constexpr std::array<std::string_view, 7> days = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
            static constexpr std::array<std::string_view, 12> months = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
            // determine what day of the week the first day of the month is
            auto const now = std::chrono::system_clock::now();
            auto const now_time = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_s(&tm, &now_time);
            auto const current_month = tm.tm_mon;
            auto const current_year = 1900 + tm.tm_year;
            auto const current_day_of_month = tm.tm_mday;
            tm.tm_mday = 1;
            tm.tm_hour = 0;
            tm.tm_min = 0;
            tm.tm_sec = 0;
            std::mktime(&tm);
            auto const start_of_month{std::chrono::system_clock::from_time_t(std::mktime(&tm))};
            auto const first_day_of_month = tm.tm_wday;

            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
            auto const month_title{std::format("{} {}", months[current_month], current_year)};
            ImGui::TextUnformatted(month_title.c_str());
            ImGui::PopFont();

            if (ImGui::BeginTable("Month-View", 7, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
            {
                for (auto const day : days)
                {
                    ImGui::TableSetupColumn(day.data());
                }
                ImGui::TableHeadersRow();
                for (int i = 0; i < first_day_of_month; ++i)
                {
                    ImGui::TableNextColumn();
                }
                // determine how many days are in the month
                tm.tm_mday = 32;
                std::mktime(&tm);
                auto const days_in_month = 32 - tm.tm_mday;
                auto this_day {start_of_month};
                for (int i = 1; i <= days_in_month; ++i)
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", i);
                    if (i == current_day_of_month)
                    {
                        ImGui::SameLine();
                        ImGui::TextUnformatted("Today");
                    }
                    ImGui::Separator();
                    auto const next_day {this_day + std::chrono::hours(24)};
                    auto events_this_day = events.in_range(this_day, next_day);
                    if (!events_this_day.empty())
                    {
                        // get all different locations
                        std::set<std::string> locations;
                        for (auto const &it : events_this_day)
                        {
                            if (!it.location.empty()) {
                                locations.emplace(it.location);
                            }
                        }
                        if (!locations.empty())
                        {
                            ImGui::TextUnformatted("@ ");
                            for (auto const &location : locations)
                            {
                                ImGui::SameLine();
                                ImGui::Text(" %s", location.c_str());
                            }
                        }
                        for (auto const &it : events_this_day)
                        {
                            std::string const time_range{
                                (it.end - it.start) > std::chrono::hours(23) ? "" : std::format("{:%H:%M} - {:%H:%M} ", it.start, it.end)}; 
                            ImGui::TextUnformatted(time_range.c_str());
                            ImGui::SameLine();
                            ImGui::TextUnformatted(it.summary.c_str());
                        }
                    }
                    this_day = next_day;
                }
                ImGui::EndTable();
            }
        };

    private:
        host &host_;
    };
}