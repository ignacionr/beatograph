#pragma once

#include "host_local.hpp"

namespace hosting::local
{
    struct screen
    {
        screen(hosting::local::host &host) : host(host) {}
        void render()
        {
            if (ImGui::CollapsingHeader("General Information about the Host"))
            {
                if (ImGui::BeginChild("Local"))
                {
                    ImGui::Text("Hostname: %s", host.HostName().c_str());
                    ImGui::EndChild();
                }
            }
        }

    private:
        hosting::local::host &host;
    };
}