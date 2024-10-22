#pragma once

#include <imgui.h>

struct importer_report {
    void render() {
        if (ImGui::CollapsingHeader("Importer Report", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Our importer runs on a docker container");
            if (ImGui::CollapsingHeader("Importer Docker Host Status")) {
                
            }
        }
    }
};