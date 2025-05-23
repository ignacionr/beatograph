#pragma once

#include <array>
#include <chrono>
#include <ctime>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "../../structural/views/cached_view.hpp"
#include "host.hpp"

namespace calendar
{
    struct screen
    {
        screen(std::shared_ptr<host> h) : host_(h) {
            today_ = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
            set_date(today_);
        }

        void render() noexcept
        {
            views::cached_view<event_set>("calendar", 
                [this] { return host_->get_events(); }, 
                [this](auto const &events) { render_calendar(events); }, true);
        }

        void set_date(std::chrono::system_clock::time_point date) {
            selected_day_ = date;
            // determine what day of the week the first day of the month is
            auto const now_time = std::chrono::system_clock::to_time_t(date);
            std::tm tm;
            localtime_s(&tm, &now_time);
            current_month_ = tm.tm_mon;
            current_year_ = 1900 + tm.tm_year;
            tm.tm_mday = 1;
            tm.tm_hour = 0;
            tm.tm_min = 0;
            tm.tm_sec = 0;
            std::mktime(&tm);
            start_of_month_ = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            first_day_of_month_ = tm.tm_wday;
            // determine how many days are in the month
            tm.tm_mday = 32;
            std::mktime(&tm);
            days_in_month_ = 32 - tm.tm_mday;
        }

        void render_calendar(auto const &events)
        {
            static constexpr std::array<std::string_view, 7> days = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
            static constexpr std::array<std::string_view, 12> months = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};


            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
            auto const month_title{std::format("{} {}", months[current_month_], current_year_)};
            ImGui::TextUnformatted(month_title.c_str());
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::SetCursorPosX(235.0f);

            struct label_and_fn_t {
                std::string_view label;
                std::function<void()> action;
                ImGuiKey key;
            };

            for (auto const &[text, callback, key] : std::array<label_and_fn_t, 5>{
                     label_and_fn_t{ICON_MD_SKIP_PREVIOUS, [this] { selected_day_ -= std::chrono::days(week_view_ ? 7 : days_in_month_); }, ImGuiKey_LeftArrow},
                     label_and_fn_t{ICON_MD_SKIP_NEXT, [this] { selected_day_ += std::chrono::days(week_view_ ? 7 : days_in_month_); }, ImGuiKey_RightArrow},
                     label_and_fn_t{ICON_MD_TODAY, [this] { selected_day_ = today_; }, ImGuiKey_Home},
                     label_and_fn_t{ICON_MD_CALENDAR_VIEW_MONTH, [this] { week_view_ = false; }, ImGuiKey_M},
                     label_and_fn_t{ICON_MD_CALENDAR_VIEW_WEEK, [this] { week_view_ = true; }, ImGuiKey_W}
                })
            {
                if (ImGui::SmallButton(text.data()) || ImGui::IsKeyPressed(key))
                {
                    callback();
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
            if (ImGui::BeginTable("Month-View", 7, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
            {
                for (auto const day : days)
                {
                    ImGui::TableSetupColumn(day.data());
                }
                ImGui::TableHeadersRow();
                auto const default_bg_color = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
                if (week_view_) {
                    // auto tz {std::chrono::current_zone()};
                    auto this_day {selected_day_ - std::chrono::days((
                        std::chrono::floor<std::chrono::days>(selected_day_).time_since_epoch().count() - 3)
                        % 7)};
                    const auto day_height {ImGui::GetWindowHeight() / 1.3f};
                    for (int i = 0; i < 7; ++i)
                    {
                        ImGui::TableNextColumn();
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, (this_day == today_) ? ImGui::GetStyleColorVec4(ImGuiCol_NavHighlight) : default_bg_color);
                        ImGui::BeginChild(ImGui::GetID(reinterpret_cast<void*>(static_cast<long long>(i))), {0, day_height}, ImGuiChildFlags_FrameStyle);
                        ImGui::TextUnformatted(std::format("{:%d}", this_day).c_str());
                        if (this_day == today_)
                        {
                            ImGui::SameLine();
                            ImGui::TextUnformatted("Today");
                        }
                        ImGui::Separator();
                        auto const next_day = this_day + std::chrono::days(1);
                        std::vector<calendar::entry const *> events_this_day;
                        events.in_range(this_day, next_day, [&events_this_day](auto const &entry) {
                            events_this_day.push_back(&entry);
                        });
                        if (!events_this_day.empty())
                        {
                            // get all different locations
                            std::set<std::string> locations;
                            for (auto const it : events_this_day)
                            {
                                if (!it->location.empty()) {
                                    locations.emplace(it->location);
                                }
                            }
                            if (!locations.empty())
                            {
                                for (auto const &location : locations)
                                {
                                    ImGui::TextUnformatted("@ ");
                                    ImGui::SameLine();
                                    ImGui::TextWrapped(" %s", location.c_str());
                                }
                            }
                            for (auto const it : events_this_day)
                            {
                                std::string const time_range{
                                    (it->end - it->start) > std::chrono::hours(23) ? "" : std::format("{:%H:%M} - {:%H:%M} ", it->start, it->end)}; 
                                ImGui::TextUnformatted(time_range.data(), time_range.data() + time_range.size());
                                ImGui::SameLine();
                                ImGui::TextWrapped("%s", it->summary.c_str());
                            }
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                        this_day = next_day;
                    }
                }
                else {
                    for (int i = 0; i < first_day_of_month_; ++i)
                    {
                        ImGui::TableNextColumn();
                    }
                    auto this_day {start_of_month_};
                    const auto day_height {ImGui::GetWindowHeight() / 7.0f};
                    for (int i = 1; i <= days_in_month_; ++i)
                    {
                        ImGui::TableNextColumn();
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, (this_day == today_) ? ImGui::GetStyleColorVec4(ImGuiCol_NavHighlight) : default_bg_color);
                        ImGui::BeginChild(i, {0, day_height}, ImGuiChildFlags_FrameStyle);
                        ImGui::Text("%d", i);
                        if (this_day == today_)
                        {
                            ImGui::SameLine();
                            ImGui::TextUnformatted("Today");
                        }
                        ImGui::Separator();
                        auto const next_day {this_day + std::chrono::days(1)};
                        std::vector<calendar::entry const *> events_this_day;
                        events.in_range(this_day, next_day, [&events_this_day](auto const &entry) {
                            events_this_day.push_back(&entry);
                        });
                        if (!events_this_day.empty())
                        {
                            // get all different locations
                            std::set<std::string> locations;
                            for (auto it : events_this_day)
                            {
                                if (!it->location.empty()) {
                                    locations.emplace(it->location);
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
                            for (auto it : events_this_day)
                            {
                                std::string const time_range{
                                    (it->end - it->start) > std::chrono::hours(23) ? std::string{} : std::format("{:%H:%M} - {:%H:%M} ", it->start, it->end)}; 
                                ImGui::TextUnformatted(time_range.c_str());
                                ImGui::SameLine();
                                ImGui::TextUnformatted(it->summary.c_str());
                            }
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                        this_day = next_day;
                    }
                }
                ImGui::EndTable();
            }
        };

    private:
        std::shared_ptr<host> host_;
        std::chrono::system_clock::time_point selected_day_;
        std::chrono::system_clock::time_point today_;
        bool week_view_ {true};
        int current_month_;
        int current_year_;
        int days_in_month_;
        int first_day_of_month_;
        std::chrono::system_clock::time_point start_of_month_;
    };
}