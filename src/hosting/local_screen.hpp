#pragma once

#include "host_local.hpp"
#include "../views/cached_view.hpp"
#include "../views/json.hpp"

namespace hosting::local
{
    struct screen
    {
        screen(hosting::local::host &host) : host(host) {}
        void render() const
        {
            ImGui::Text("Hostname: %s", host.HostName().c_str());
            views::cached_view<nlohmann::json::object_t>("IP and Location", 
            [&]() {
                return host.get_my_ip_and_geolocation();
            },
            [&](nlohmann::json::object_t const &location) {
                json_viewer.render(location);
            });
        }

    private:
        hosting::local::host &host;
        views::json json_viewer;
    };
}