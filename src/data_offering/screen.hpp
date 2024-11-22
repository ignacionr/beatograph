#pragma once

#include <array>
#include <filesystem>
#include <format>
#include <memory>
#include <string>

#include <imgui.h>
#include <cppgpt/cppgpt.hpp>
#include "../hosting/host.hpp"
#include "../hosting/ssh_screen.hpp"
#include "../views/assertion.hpp"
#include "../views/json.hpp"
#include "../hosting/local_mapping.hpp"
#include "apiclient.hpp"

extern "C"
{
#include <tinyfiledialogs.h>
}

namespace dataoffering
{
    struct screen
    {
        static constexpr auto zookeeper_name = "storm-zookeeper";
        static constexpr auto nimbus_name = "storm-nimbus";
        static constexpr auto supervisor_name = "storm-supervisor";
        static constexpr auto ui_name = "storm-ui";
        static constexpr auto topology_name = "betmavrik-topology";
        static constexpr auto topology_class = "com.betmavrik.storm.TopologyMain";
        static constexpr auto hadoop_host{"hadoop1"};
        static constexpr auto storm_host{"storm1"};
        static constexpr auto hadoop_zookeeper_name{"hadoop-zookeeper-1"};

        screen(hosting::local::host &localhost, std::string const &groq_api_key, std::string_view api_client_key)
            : localhost_{localhost}, groq_api_key_{groq_api_key}
        {
            client = std::make_unique<api_client>(api_client_key);
        }

        void render()
        {
            if (ImGui::CollapsingHeader("API")) {
                static char buff[100]{};
                ImGui::InputText("Item ID", buff, sizeof(buff));
                views::cached_view<std::string>("api", 
                    [this]() -> std::string {
                        return client->get_entity(std::stoll(buff));
                    },
                    [this](std::string result) {
                        json_view.render(result);
                    });
            }
        }

    private:
        hosting::local::host &localhost_;
        hosting::ssh::screen host_screen_;
        std::unique_ptr<hosting::local::mapping> storm_ui_mapping_;
        std::string topology_install_result_;
        bool installing_topology_{false};
        std::string groq_api_key_;
        std::unique_ptr<api_client> client;
        views::json json_view;
    };
}