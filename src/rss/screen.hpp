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
                const auto side {(ImGui::GetWindowWidth() - 50) / 5};
                const ImVec2 button_size(side, side);
                int i {0};
                for (auto feed : host_.feeds())
                {
                    if (!feed->feed_image_url.empty())
                    {
                        auto texture = cache_.load_texture_from_url(feed->feed_image_url);
                        ImGui::Image(reinterpret_cast<void *>(static_cast<uintptr_t>(texture)), button_size);
                        if (++i % 5 != 0)
                        {
                            ImGui::SameLine();
                        }
                    }
                }
            }
            ImGui::EndChild();
        }

    private:
        host &host_;
        player_t player_;
        img_cache &cache_;
    };
}
