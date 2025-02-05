#pragma once

#include "host.hpp"

#include <imgui.h>

namespace database
{
    struct screen
    {
        void render() {
            ImGui::Text("Database");
        }
    };
} // namespace database::screen