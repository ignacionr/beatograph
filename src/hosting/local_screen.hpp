#pragma once

#include "host_local.hpp"
#include "../structural/views/cached_view.hpp"
#include "../structural/views/json.hpp"
#include "../structural/views/keyval_view.hpp"

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
            views::cached_view<std::map<std::string, std::string>>("Environment Variables",
            [this]() {
                std::map<std::string, std::string> env_vars;
                host->scan_environment([&env_vars](std::string_view key, std::string_view value) {
                    env_vars.emplace(std::string{key}, std::string{value});
                });
                return env_vars;
            },
            [this](std::map<std::string, std::string> const &env_vars) {
                views::keyval(env_vars, "Var", "Value");
            });
        }

    private:
        std::shared_ptr<hosting::local::host> host;
        views::json json_viewer;
    };
}