#pragma once

#include <imgui.h>

#include "host.hpp"

namespace rss {
    struct screen {
        screen(host& host) : host_{host} {}
        void render() {
            if (ImGui::BeginChild("RSS")) {
                if (ImGui::CollapsingHeader("Feeds")) {
                    for (auto feed : host_.feeds()) {
                        if (ImGui::TreeNode(feed->feed_title.c_str())) {
                            if (ImGui::Button("Open")) {
                                ::ShellExecuteA(nullptr, 
                                    "open", 
                                    feed->feed_link.c_str(), 
                                    nullptr, 
                                    nullptr, 
                                    SW_SHOWNORMAL);
                            }
                            ImGui::TreePop();
                        }
                    }
                }
            }
            ImGui::EndChild();
        }
    private:
        host& host_;
    };
}
