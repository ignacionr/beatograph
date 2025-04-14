#pragma once

#include <imgui.h>

namespace cloud::deel
{
    struct screen {
        void render(auto &) const noexcept {
            ImGui::Text("Deel screen is not implemented yet.");
        }
    };
} // namespace cloud::deel
