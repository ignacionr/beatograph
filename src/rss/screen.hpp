#pragma once

#include <functional>
#include <set>
#include <string>

#include <imgui.h>

#include "host.hpp"
#include "../imgcache.hpp"

namespace rss
{
    struct screen
    {
        using player_t = std::function<void(std::string_view)>;
        screen(host &host, player_t player, img_cache &cache) : host_{host}, player_{player}, cache_{cache} {}

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

            if (ImGui::BeginTable("##items", 1, ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Title");
                ImGui::TableHeadersRow();
                auto draw_list = ImGui::GetWindowDrawList();
                for (auto &item : current_feed_->items)
                {
                    ImGui::TableNextRow();
                    ImGui::PushID(&item);
                    ImGui::TableNextColumn();
                    auto starting_pos = ImGui::GetCursorScreenPos();
                    if (!item.enclosure.empty())
                    {
                        constexpr ImU32 blue = IM_COL32(100, 100, 255, 255);
                        constexpr ImU32 white = IM_COL32(255, 255, 255, 255);
                        draw_list->AddCircleFilled({starting_pos.x + 11, starting_pos.y + 9}, 7, white);
                        draw_list->AddCircleFilled({starting_pos.x + 12, starting_pos.y + 10}, 6, blue);
                        draw_list->AddTriangleFilled(
                            {starting_pos.x + 9, starting_pos.y + 7},
                            {starting_pos.x + 9, starting_pos.y + 13},
                            {starting_pos.x + 15, starting_pos.y + 10},
                            white);
                    }
                    ImGui::Indent();
                    ImGui::TextUnformatted(item.title.c_str());
                    ImGui::Unindent();
                    auto const mouse_pos = ImGui::GetMousePos();
                    ImVec2 const row_max{
                        starting_pos.x + ImGui::GetColumnWidth(),
                        tablestart_y + ImGui::GetCursorPosY() - ImGui::GetScrollY()};
                    // hovering at any part of the row?
                    if (mouse_pos.x >= starting_pos.x && mouse_pos.x <= row_max.x &&
                        mouse_pos.y >= (starting_pos.y - ImGui::GetScrollY()) && mouse_pos.y <= row_max.y)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        auto data = ImGui::GetWindowDrawList();
                        data->AddRect(starting_pos, row_max, IM_COL32(255, 255, 0, 255));
                        if (ImGui::IsMouseClicked(0) && !item.enclosure.empty())
                        {
                            player_(item.enclosure);
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }

        void render()
        {
            if (current_feed_)
            {
                if (ImGui::Button("Back"))
                {
                    current_feed_.reset();
                }
                else if (ImGui::SameLine(); ImGui::Button("Refresh"))
                {
                    // we will re-add the selected feed into the host
                    host_.add_feed(current_feed_->feed_link);
                }
                else
                {
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
                    constexpr float side_length{75};
                    const unsigned col_count{static_cast<unsigned>((ImGui::GetWindowWidth() - 5) / (side_length + 8))};
                    const ImVec2 button_size(side_length, side_length);
                    int i{0};
                    for (auto feed : host_.feeds())
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
                            if (++i % col_count != 0)
                            {
                                ImGui::SameLine();
                            }
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                                auto data = ImGui::GetWindowDrawList();
                                data->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 255, 0, 255));
                                if (ImGui::IsItemClicked())
                                {
                                    current_feed_ = feed;
                                }
                            }
                            ImGui::PopID();
                        }
                    }
                }
                ImGui::EndChild();
            }
        }

    private:
        host &host_;
        player_t player_;
        img_cache &cache_;
        std::shared_ptr<rss::feed> current_feed_;
    };
}
