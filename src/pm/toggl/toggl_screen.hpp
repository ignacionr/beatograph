#pragma once
#include <atomic>
#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <time.h>

#include "toggl_client.hpp"
#include "../external/IconsMaterialDesign.h"
#include "../../registrar.hpp"
#include "login/screen.hpp"

namespace toggl
{
    struct screen
    {
        screen(std::shared_ptr<client> client_ptr, int daily_second_target) 
        : client_{client_ptr}, seconds_daily_target_{daily_second_target} {}

        void render_entry(auto const &entry, auto &current_day, auto &current_day_seconds)
        {
            ImGui::PushID(&entry);
            struct std::tm tm = {};
            {
                std::istringstream ss(entry["start"].get<std::string>());
                ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            }
            auto start_utc = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            auto start = start_utc + std::chrono::seconds(utc_offset_seconds);
            auto day_start = std::chrono::floor<std::chrono::days>(start);
            if (current_day != day_start)
            {
                if (current_day_seconds != 0) // only days with at least one entry
                {
                    auto duration = std::chrono::seconds(current_day_seconds);
                    auto duration_formatted = std::format("{:%H:%M:%S}", duration);
                    float percentage = 100.0f * current_day_seconds / (seconds_daily_target_);
                    // paint the percentage in red if it's less than 100%
                    if (percentage < 100.0)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::ProgressBar(percentage / 100.0f, ImVec2(100.0, 20.0));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Red color
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("Achieved: %.2f%%", percentage);

                    ImGui::TableNextColumn();
                    ImGui::Text("Total: %s", duration_formatted.c_str());
                    if (percentage < 100.0)
                    {
                        ImGui::PopStyleColor();
                    }
                    current_day_seconds = 0;
                }
                current_day = day_start;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 1.0f, 1.0f)); // Light blue color
                ImGui::TextUnformatted(std::format("{:%A, %Y-%m-%d}", current_day).c_str());
                ImGui::PopStyleColor();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
            }
            auto description = entry["description"].get<std::string>();
            auto local_start = std::chrono::current_zone()->to_local(start);
            auto start_formatted = std::format("{:%H:%M}", local_start);
            std::string duration_formatted;
            auto duration_in_seconds = entry["duration"].get<long long>();
            bool const is_running{duration_in_seconds < 0};
            if (is_running)
            {
                auto utc_now = std::chrono::system_clock::now();
                duration_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                          std::chrono::system_clock::now() - start)
                                          .count();
            }
            current_day_seconds += duration_in_seconds;
            auto duration = std::chrono::seconds(duration_in_seconds);
            duration_formatted = std::format("{:%H:%M:%S}", duration);

            // show the entry on a table with two columns
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", start_formatted.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", description.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", duration_formatted.c_str());
            if (is_running)
            {
                if (ImGui::SameLine(); ImGui::SmallButton(ICON_MD_STOP))
                {
                    try
                    {
                        client_->stopTimeEntry(entry);
                    }
                    catch (std::exception const &e)
                    {
                        std::string error_message = std::format("Failed to stop time entry: {}", e.what());
                        std::cerr << "Error: " << error_message << std::endl;
                    }
                }
            }
            if (ImGui::SameLine(); ImGui::SmallButton(ICON_MD_DELETE))
            {
                try
                {
                    client_->deleteTimeEntry(entry);
                    query();
                }
                catch (std::exception const &e)
                {
                    std::string error_message = std::format("Failed to delete time entry: {}", e.what());
                    std::cerr << "Error: " << error_message << std::endl;
                }
            }
            ImGui::PopID();
        }

        void query()
        {
            try
            {
                time_entries = std::make_shared<nlohmann::json>(client_->getTimeEntries());
            }
            catch (const client::config_error &) {
                config_mode_ = true;
            }
            catch (const std::exception &e)
            {
                time_entries = std::make_shared<nlohmann::json>(nlohmann::json::string_t(e.what()));
            }
        }

        void start_time_entry(std::string_view description) {
            client_->startTimeEntry(current_workspace_id(), description);
            query();
        }

        void render() {
            if (config_mode_) {
                if (new_login_) {
                    if (login_name_.reserve(200); ImGui::InputText("New Login Name", login_name_.data(), login_name_.capacity())) {
                        login_name_ = login_name_.data();
                    }
                    // show the login config UI
                    login_screen_->render();
                }
                else {
                    if (ImGui::BeginCombo("Login", login_name_.c_str())) {
                        registrar::keys<login::host>([this](const std::string &option){
                            if (ImGui::Selectable(option.c_str())) {
                                login_name_ = option;
                            }
                        });
                        if (ImGui::Selectable("-- create a new one")) {
                            login_name_.clear();
                            new_login_ = true;
                            login_screen_ = std::make_unique<login::screen>(std::make_shared<login::host>());
                        }
                        ImGui::EndCombo();
                    }
                }
                if (ImGui::SmallButton(ICON_MD_SAVE " Save")) {
                    if (new_login_) {
                        registrar::add<login::host>(login_name_, login_screen_->host());
                        new_login_ = false;
                    }
                    config_mode_ = false;
                    client_->set_login(registrar::get<login::host>(login_name_));
                    query();
                }
            }
            else {
                render_contents();
                if (ImGui::SmallButton(ICON_MD_SETTINGS)) {
                    config_mode_ = true;
                }
            }
        }

        void render_contents()
        {
            if (std::chrono::system_clock::now() - last_update > std::chrono::minutes(1))
            {
                last_update = std::chrono::system_clock::now();
                std::thread([this]
                            { query(); })
                    .detach();

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
            // entries look like this:
            // {"id":3653218672,"workspace_id":8741535,"project_id":205625477,"task_id":null,"billable":false,"start":"2024-10-20T06:00:00+00:00","stop":"2024-10-20T08:14:51+00:00","duration":8091,"description":"Company Meeting","duronly":true,"at":"2024-10-20T08:14:54+00:00","user_id":11300927,"uid":11300927}
            if (time_entries.load())
            {
                auto entries = time_entries.load();
                if (entries->is_array())
                {
                    if (ImGui::BeginTable("TimeEntriesTable", 3, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_RowBg))
                    {
                        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                        std::chrono::time_point<std::chrono::system_clock, std::chrono::days> current_day;
                        time_t current_day_seconds = 0;
                        for (const auto &entry : *entries)
                        {
                            render_entry(entry, current_day, current_day_seconds);
                        }
                        ImGui::EndTable();
                    }
                    ImGui::Columns(2);
                    if (ImGui::Button("Open Web"))
                    {
                        ShellExecuteA(nullptr, "open", "https://track.toggl.com/", nullptr, nullptr, SW_SHOW);
                    }
                    ImGui::NextColumn();
                    if (new_description.reserve(256); ImGui::InputText("New Task", new_description.data(), new_description.capacity(), ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        try
                        {
                            new_description = new_description.data();
                            start_time_entry(new_description);
                            new_description.clear();
                        }
                        catch (std::exception const &e)
                        {
                            std::string error_message = std::format("Failed to start time entry: {}", e.what());
                            std::cerr << "Error: " << error_message << std::endl;
                        }
                    }
                    ImGui::Columns();
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Red color
                    ImGui::Text(time_entries.load()->dump().c_str());
                    ImGui::PopStyleColor();
                }
            }
            else
            {
                ImGui::Text("Loading...");
            }
            ImGui::Columns();
        }

        long long current_workspace_id()
        {
            if (!time_entries.load()) {
                query();
            }
            return time_entries.load()->front()["workspace_id"].get<long long>();
        }

    private:
        std::shared_ptr<client> client_;
        std::chrono::system_clock::time_point last_update;
        std::atomic<std::shared_ptr<nlohmann::json>> time_entries;
        time_t utc_offset_seconds{};
        std::string new_description;
        int seconds_daily_target_;
        bool config_mode_{};
        bool new_login_{};
        std::string login_name_;
        std::unique_ptr<login::screen> login_screen_;
    };
}