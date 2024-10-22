#pragma once

#include <ranges>
#include <imgui.h>
#include "host.hpp"

struct host_screen
{
    void render(host &host)
    {
        // Render host screen
        ImGui::LabelText("Hostname", "%s", host.name().c_str());
        auto interesting_keys = std::array<std::string_view, 5>{"host", "hostname", "user", "port", "identityfile"};
        auto filtered_properties = host.properties() |
                                   std::views::filter([&](auto const &pair)
                                                      { return std::ranges::find(interesting_keys, pair.first) != interesting_keys.end(); });
        for (auto const &[key, value] : filtered_properties) {
            ImGui::LabelText(key.c_str(), "%s", value.c_str());
        }
    }
};
