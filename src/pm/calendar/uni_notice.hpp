#pragma once

#include <algorithm>
#include <chrono>
#include <format>
#include <vector>

#include <imgui.h>

#include "host.hpp"
#include "../../registrar.hpp"
#include "../external/IconsMaterialDesign.h"

namespace calendar
{
    struct uni_notice {
        void render() {
            static int frame_count = 6000;
            if (++frame_count >= 6000) {
                frame_count = 0;
                refresh();
            }
            if (entries.empty()) {
                ImGui::Text("No upcoming events");
            } else {
                for (auto const &e : entries) {
                    auto description = std::format("{}: {}", e.start, e.summary);
                    ImGui::TextUnformatted(description.c_str(), description.c_str() + description.size());
                }
            }
            if (ImGui::SmallButton(ICON_MD_REFRESH)) {
                refresh();
            }
        }
        void refresh() {
            // obtain all entries from all calendars, that match the current time
            registrar::all<host>([this](std::string const&, auto host) {
                auto time_now = std::chrono::system_clock::now();
                auto events = host->get_events().in_range(time_now, time_now);
                entries.clear();
                for (auto const &e : events) {
                    entries.push_back(e);
                }
            });
        }
        std::vector<entry> entries;
    };
} // namespace pm::calendar
