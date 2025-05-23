#pragma once

#include <chrono>
#include <format>
#include <list>
#include <mutex>
#include <string>

#include <imgui.h>

#include "host.hpp"

namespace notify
{
    struct screen
    {
        struct notification_t
        {
            std::chrono::system_clock::time_point time;
            std::string text;
            std::string topic;
        };

        screen(host &h)
        {
            h.sink([this](std::string_view text, std::string_view topic)
               { 
                    std::lock_guard lock{notifications_mutex_};
                    notifications_.emplace_front(notification_t{std::chrono::system_clock::now(), std::string{text}, std::string{topic}}); 
                });
        }

        void render() noexcept
        {
            if (ImGui::BeginTable("Notifications", 3, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Topic", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthStretch);
                std::lock_guard lock{notifications_mutex_};
                ImGui::TableHeadersRow();
                for (auto const &n: notifications_) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto const time {std::format("{:%F %T}", n.time)};
                    ImGui::TextUnformatted(time.data(), time.data() + time.size());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(n.topic.data(), n.topic.data() + n.topic.size());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", n.text.data());
                }
            }
            ImGui::EndTable();
        }

    private:
        std::list<notification_t> notifications_;
        std::mutex notifications_mutex_;
    };
}