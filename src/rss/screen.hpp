#pragma once

#include <functional>
#include <string>

#include <imgui.h>

#include "host.hpp"

namespace rss
{
    struct screen
    {
        using player_t = std::function<void(std::string_view)>;

        screen(host &host, player_t player) : host_{host}, player_{player} {}
        void render()
        {
            if (ImGui::BeginChild("RSS"))
            {
                if (ImGui::CollapsingHeader("Feeds"))
                {
                    for (auto feed : host_.feeds())
                    {
                        if (ImGui::TreeNode(feed->feed_title.c_str()))
                        {
                            if (ImGui::Button("Open"))
                            {
                                ::ShellExecuteA(nullptr,
                                                "open",
                                                feed->feed_link.c_str(),
                                                nullptr,
                                                nullptr,
                                                SW_SHOWNORMAL);
                            }
                            for (auto item : feed->items)
                            {
                                if (ImGui::TreeNode(item.title.c_str()))
                                {
                                    ImGui::TextWrapped("%s", item.description.c_str());
                                    if (ImGui::Button("Open"))
                                    {
                                        ::ShellExecuteA(nullptr,
                                                        "open",
                                                        item.link.c_str(),
                                                        nullptr,
                                                        nullptr,
                                                        SW_SHOWNORMAL);
                                    }
                                    if (!item.enclosure.empty())
                                    {
                                        ImGui::SameLine();
                                        if (ImGui::Button("Play"))
                                        {
                                            player_(item.enclosure);
                                        }
                                    }
                                    ImGui::TreePop();
                                }
                            }
                            ImGui::TreePop();
                        }
                    }
                }
            }
            ImGui::EndChild();
        }

    private:
        host &host_;
        player_t player_;
    };
}
