#pragma once

#include "host_local.hpp"
#include "../structural/views/cached_view.hpp"
#include "../structural/views/json.hpp"

namespace hosting::local
{
    struct screen
    {
        screen(std::shared_ptr<hosting::local::host> host) : host(host) {}

        void render() const noexcept
        {
            ImGui::Text("Hostname: %s", host->HostName().c_str());
            views::cached_view<nlohmann::json::object_t>("IP and Location", 
            [this]() {
                return host->get_my_ip_and_geolocation();
            },
            [this](nlohmann::json::object_t const &location) {
                json_viewer.render(location);
            });
        }

    private:
        std::shared_ptr<hosting::local::host> host;
        views::json json_viewer;
    };
}