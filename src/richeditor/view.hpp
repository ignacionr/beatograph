#pragma once

#include <string>

#include <imgui.h>

namespace richeditor {
    struct document {
        std::string test{"test"};
    };

    bool render(document &doc) {
        ImGui::TextUnformatted(doc.test.c_str());
        return false;
    }
}