#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <imgui.h>

#include "config.hpp"

namespace panel {
    struct screen {
        void render(config const &panel) {
            panel.render();
        }
    };
}