#pragma once

#include <functional>
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
        void render()
        {
            if (ImGui::BeginChild("RSS", ImVec2{ImGui::GetWindowWidth()-20, 200}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar))
            {
                constexpr float side_length{75};
                const unsigned col_count {static_cast<unsigned>((ImGui::GetWindowWidth() - 50) / side_length)};
                const ImVec2 button_size(side_length,side_length);
                int i {0};
                for (auto feed : host_.feeds())
                {
                    if (!feed->feed_image_url.empty())
                    {
                        ImGui::PushID(feed->feed_link.c_str());
                        auto texture = cache_.load_texture_from_url(feed->feed_image_url);
                        ImGui::Image(reinterpret_cast<void *>(static_cast<uintptr_t>(texture)), button_size);
                        if (++i % col_count != 0)
                        {
                            ImGui::SameLine();
                        }
                        if (ImGui::IsItemHovered())
                        {
                            auto data = ImGui::GetWindowDrawList();
                            data->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 255, 0, 255));
                            if (ImGui::IsItemClicked()) {
                                current_feed_ = feed;
                            }
                        }
                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndChild();
            if (current_feed_)
            {
                ImGui::Columns(2);
                if (!current_feed_->feed_image_url.empty())
                {
                    ImGui::Image(reinterpret_cast<void *>(static_cast<uintptr_t>(
                        cache_.load_texture_from_url(current_feed_->feed_image_url))), 
                        ImVec2{ImGui::GetColumnWidth(), ImGui::GetColumnWidth()});
                }
                ImGui::TextWrapped("%s", current_feed_->feed_title.c_str());
                ImGui::TextWrapped("%s", current_feed_->feed_description.c_str());
                ImGui::NextColumn();
                for (auto &item : current_feed_->items)
                {
                    if (ImGui::Button(item.title.c_str()))
                    {
                        player_(item.enclosure);
                    }
                }
                ImGui::Columns();
            }
        }

    private:
        host &host_;
        player_t player_;
        img_cache &cache_;
        std::shared_ptr<rss::feed> current_feed_;
    };
}
