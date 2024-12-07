#pragma once

#include <format>
#include <functional>
#include <set>
#include <string>

#include <imgui.h>

#include "host.hpp"
#include "../imgcache.hpp"

#pragma execution_character_set("utf-8")
#include "../../external/IconsMaterialDesign.h"

namespace rss
{
    struct screen
    {
        using player_t = std::function<void(std::string_view)>;
        using system_runner_t = std::function<std::string(std::string_view)>;
        screen(std::shared_ptr<host> host, player_t player, img_cache &cache, system_runner_t system_runner)
         : host_{host}, player_{player}, cache_{cache}, system_runner_{system_runner} {}

        void render_flag()
        {
            // show flag
            ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() / 3);
            if (!current_feed_->feed_image_url.empty())
            {
                try
                {
                    ImGui::Image(reinterpret_cast<void *>(static_cast<uintptr_t>(
                                     cache_.load_texture_from_url(current_feed_->feed_image_url))),
                                 ImVec2{ImGui::GetColumnWidth(), ImGui::GetColumnWidth()});
                }
                catch (...)
                {
                    // ImGui::Text("Failed to load image");
                }
            }
            ImGui::TextWrapped("%s", current_feed_->feed_title.c_str());
            ImGui::TextWrapped("%s", current_feed_->feed_description.c_str());
        }

        void render_list()
        {
            auto const tablestart_y {ImGui::GetCursorPosY()};
            ImGui::BeginChild("##selectedfeed_items_table",
                              ImVec2{
                                  ImGui::GetWindowWidth() - ImGui::GetCursorPosX() - 20,
                                  ImGui::GetWindowHeight() - tablestart_y - 20});

            if (ImGui::BeginTable("##items", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Updated", ImGuiTableColumnFlags_WidthFixed, 112);
                ImGui::TableHeadersRow();
                for (auto &item : current_feed_->items)
                {
                    if (!filter_.empty() && item.title.find(filter_) == std::string::npos)
                    {
                        continue;
                    }
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    std::string item_text;
                    if (!item.enclosure.empty())
                    {
                        item_text = ICON_MD_PLAY_CIRCLE " ";
                    }
                    item_text += item.title;
                    if (ImGui::Selectable(item_text.c_str()))
                    {
                        if (!item.enclosure.empty())
                        {
                            if (item.enclosure.find("youtube.com") != std::string::npos)
                            {
                                item.enclosure = system_runner_(std::format("yt-dlp -f bestaudio -g {}", item.enclosure));
                            }
                            player_(item.enclosure);
                        }
                    }
                    ImGui::TableNextColumn();
                    auto updated = std::format("{:%Y-%m-%d %H:%M}", item.updated);
                    ImGui::TextUnformatted(updated.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }

        void render()
        {
            if (current_feed_)
            {
                if (ImGui::Button(ICON_MD_ARROW_BACK))
                {
                    current_feed_.reset();
                }
                else if (ImGui::SameLine(); ImGui::Button(ICON_MD_REFRESH))
                {
                    // we will re-add the selected feed into the host
                    host_->add_feeds({current_feed_->feed_link}, [](std::string_view) {});
                }
                else 
                {
                    if (ImGui::SameLine(), filter_.reserve(256); ImGui::InputTextWithHint("##search", "Search", filter_.data(), filter_.capacity()))
                    {
                        filter_ = filter_.data();
                    }

                    ImGui::Columns(2);
                    render_flag();
                    ImGui::NextColumn();
                    render_list();
                    ImGui::Columns();
                }
            }
            else
            {
                if (ImGui::BeginChild("RSS", ImVec2{ImGui::GetWindowWidth() - 20, ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - 20}))
                {
                    constexpr float side_length{160};
                    const unsigned col_count{static_cast<unsigned>(ImGui::GetWindowWidth() / (side_length + 10))};
                    const ImVec2 button_size(side_length, side_length);
                    int i{0};
                    for (auto feed : host_->feeds())
                    {
                        if (!feed->feed_image_url.empty())
                        {
                            ImGui::PushID(feed->feed_link.c_str());
                            unsigned int texture;
                            static std::set<std::string> failed_urls;
                            if (failed_urls.find(feed->feed_image_url) != failed_urls.end())
                            {
                                texture = cache_.load_texture_from_url("https://upload.wikimedia.org/wikipedia/commons/1/14/No_Image_Available.jpg");
                            }
                            else
                            {
                                try
                                {
                                    texture = cache_.load_texture_from_url(feed->feed_image_url);
                                }
                                catch (...)
                                {
                                    failed_urls.insert(feed->feed_image_url);
                                    texture = cache_.load_texture_from_url("https://upload.wikimedia.org/wikipedia/commons/1/14/No_Image_Available.jpg");
                                }
                            }
                            ImGui::Image(reinterpret_cast<void *>(static_cast<uintptr_t>(texture)), button_size);
                            auto data = ImGui::GetWindowDrawList();
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                                if (ImGui::IsItemClicked())
                                {
                                    current_feed_ = feed;
                                }
                            }
                            else {
                                data->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(70, 70, 0, 190));
                            }

                            if (++i % col_count != 0)
                            {
                                ImGui::SameLine();
                            }
                            ImGui::PopID();
                        }
                    }
                }
                ImGui::EndChild();
            }
        }

    private:
        std::shared_ptr<host> host_;
        player_t player_;
        img_cache &cache_;
        std::shared_ptr<rss::feed> current_feed_;
        std::string filter_;
        std::function<std::string(std::string_view)> system_runner_;
    };
}
