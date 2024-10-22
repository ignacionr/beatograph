#pragma once

#include <imgui.h>
#include "host.hpp"

struct host_screen {
    void render(host &host) {
        // Render host screen
        ImGui::LabelText("Hostname", "%s", host.name().c_str());
        for (auto const &[key, value]: host.properties()) {
            ImGui::LabelText(key.c_str(), "%s", value.c_str());
        }
    }
};
