#pragma once
#include <chrono>
#include <imgui.h>
#include <ctime>
#include <time.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <format>
#include <nlohmann/json.hpp>
#include "toggl_client.hpp"

struct toggl_screen
{
    toggl_screen(toggl_client &client) : client{client} {}

    void render()
    {
        if (std::chrono::system_clock::now() - last_update > std::chrono::minutes(1))
        {
            last_update = std::chrono::system_clock::now();
            try {
                time_entries = client.getTimeEntries();
            }
            catch (const std::exception &e) {
                time_entries = nlohmann::json::string_t(e.what());
            }

            std::time_t currentTime = std::time(nullptr);
            // Get the UTC time
            std::tm utcTime;
            gmtime_s(&utcTime, &currentTime);

            // Get the local time
            std::tm localTime;
            localtime_s(&localTime, &currentTime);

            // Calculate the difference in seconds
            std::time_t utcTimeInSeconds = std::mktime(&utcTime);
            std::time_t localTimeInSeconds = std::mktime(&localTime);
            utc_offset_seconds = localTimeInSeconds - utcTimeInSeconds;
        }
        ImGui::Text("Time entries: %d", time_entries.size());
        // entries look like this:
        // {"id":3653218672,"workspace_id":8741535,"project_id":205625477,"task_id":null,"billable":false,"start":"2024-10-20T06:00:00+00:00","stop":"2024-10-20T08:14:51+00:00","duration":8091,"description":"Company Meeting","duronly":true,"at":"2024-10-20T08:14:54+00:00","user_id":11300927,"uid":11300927}
        ImGui::Columns(3);
        std::chrono::time_point<std::chrono::system_clock, std::chrono::days> current_day;
        time_t current_day_seconds = 0;
        if (time_entries.is_array()) {
        for (const auto &entry : time_entries)
        {
            struct std::tm tm = {};
            {
                std::istringstream ss(entry["start"].get<std::string>());
                ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            }
            auto start_utc =  std::chrono::system_clock::from_time_t(std::mktime(&tm));
            auto start = start_utc + std::chrono::seconds(utc_offset_seconds);
            auto day_start = std::chrono::floor<std::chrono::days>(start);
            if (current_day != day_start) {
                if (current_day_seconds != 0) {
                    auto duration = std::chrono::seconds(current_day_seconds);
                    auto duration_formatted = std::format("{:%H:%M:%S}", duration);
                    float percentage = 100 * current_day_seconds / (3600.0f * 9.0f);
                    // paint the percentage in red if it's less than 100%
                    if (percentage < 100.0) {
                        ImGui::ProgressBar(percentage / 100.0f, ImVec2(100.0, 20.0));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Red color
                    }
                    ImGui::NextColumn();
                    ImGui::Text("Achieved: %.2f%%", percentage);

                    ImGui::NextColumn();
                    ImGui::Text("Total: %s", duration_formatted.c_str());
                    if (percentage < 100.0) {
                        ImGui::PopStyleColor();
                    }
                    ImGui::NextColumn();
                    current_day_seconds = 0;
                }
                current_day = day_start;
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 1.0f, 1.0f)); // Light blue color
                ImGui::LabelText("Day: %s", std::format("{:%Y-%m-%d}", current_day).c_str());
                ImGui::PopStyleColor();
                ImGui::NextColumn();
                ImGui::NextColumn();
                ImGui::NextColumn();
            }
            auto description = entry["description"].get<std::string>();
            auto local_start = std::chrono::current_zone()->to_local(start);
            auto start_formatted = std::format("{:%H:%M}", local_start);
            std::string duration_formatted;
            auto duration_in_seconds = entry["duration"].get<long long>();
            if (duration_in_seconds < 0) {
                auto utc_now = std::chrono::system_clock::now();
                duration_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now() - start).count();
            }
            current_day_seconds += duration_in_seconds;
            auto duration = std::chrono::seconds(duration_in_seconds);
            duration_formatted = std::format("{:%H:%M:%S}", duration);

            // show the entry on a table with two columns
            ImGui::Text("%s", start_formatted.c_str());
            ImGui::NextColumn();
            ImGui::Text("%s", description.c_str());
            ImGui::NextColumn();
            ImGui::Text("%s", duration_formatted.c_str());
            ImGui::NextColumn();
        }
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Red color
            ImGui::Text(time_entries.dump().c_str());
            ImGui::PopStyleColor();
        }
        
        ImGui::Columns();
    }
private:
    toggl_client &client;
    std::chrono::system_clock::time_point last_update;
    nlohmann::json time_entries;
    time_t utc_offset_seconds{};
};
