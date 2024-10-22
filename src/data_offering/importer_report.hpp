#pragma once

#include <imgui.h>
#include "../host/host.hpp"
#include "../host/screen.hpp"

struct importer_report {
    importer_report(host_local &localhost): 
        host_importer_("ignacio-bench"), localhost_{localhost} 
    {
        host_importer_.resolve_from_ssh_conf(localhost);
    }
    void render() 
    {
        if (ImGui::CollapsingHeader("Importer Report", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Our importer runs on a docker container");
            if (ImGui::CollapsingHeader("Importer Docker Host Status")) {
                host_screen_.render(host_importer_);
            }
        }
    }
private:
    host host_importer_;
    host_screen host_screen_;
    host_local &localhost_;
};