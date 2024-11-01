#pragma once

#include "host_local.hpp"

struct host_local_screen {
    host_local_screen(host_local &host) : host(host) {}
    void render() {
        if (ImGui::CollapsingHeader("General Information about the Host")) {
            if (ImGui::BeginChild("Local")) {
                ImGui::Text("Hostname: %s", host.HostName().c_str());
                ImGui::EndChild();
            }
        }
    }
private:
    host_local &host;
};
