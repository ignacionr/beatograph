#pragma once
#include <imgui.h>
#include "host.hpp"

namespace radio {
    struct screen {
        screen(host &h) : host_(h) {
            presets = host_.presets();
            std::transform(presets.begin(), presets.end(), std::back_inserter(presets_c_strs),
                           [](const auto &preset) { return preset.c_str(); });
        }

        void render() {
            if (host_.has_error()) {
                ImGui::Text("Error: %s", host_.last_error().c_str());
            }
            if (ImGui::ListBox("Presets", &currently_playing, presets_c_strs.data(), static_cast<int>(presets.size()))) {
                // play the selected preset
                host_.play(presets[currently_playing]);
            }
            if (host_.is_playing() && ImGui::Button("Stop"))
            {
                host_.stop();
                currently_playing = -1;
            }
        }
    private:
        std::vector<std::string> presets;
        std::vector<const char*> presets_c_strs;
        int currently_playing{-1};
        host &host_;
    };
}