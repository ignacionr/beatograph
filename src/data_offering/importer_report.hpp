#pragma once

#include <algorithm>
#include <atomic>
#include <format>
#include <memory>
#include <ranges>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include "../hosting/host_local.hpp"
#include "../hosting/host.hpp"
#include "../hosting/ssh_screen.hpp"
#include "../docker/host.hpp"
#include "../docker/screen.hpp"
#include "../views/assertion.hpp"
#include "../views/cached_view.hpp"

struct importer_report {
    static auto constexpr host_importer_name = "storm1";
    static auto constexpr importer_container_name = "storm-java-producer-1";
    static auto constexpr importer_rabbitmq = "storm-rabbitmq-1";

    importer_report(::hosting::local::host &localhost): 
        localhost_{localhost} 
    {
        host_importer_ = hosting::ssh::host::by_name(host_importer_name);
        auto t = std::thread([host = host_importer_, &localhost] {
            host->resolve_from_ssh_conf(localhost);
            try {
                host->docker().fetch_ps(localhost);
            }
            catch(std::exception const &e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        });
        t.detach();
    }

    void render() 
    {
        if (ImGui::CollapsingHeader("Importer Report", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Our importer runs on a docker container");
            views::Assertion("Importer Container running", [this] {
                return host_importer_->docker().is_container_running(importer_container_name, localhost_);
            });
            views::Assertion("Java process running in the importer container", [this] {
                return host_importer_->docker().is_process_running(importer_container_name, "java", localhost_);
            });
            views::Assertion("RabbitMQ container running", [this] {
                return host_importer_->docker().is_container_running(importer_rabbitmq, localhost_);
            });
            views::Assertion("RabbitMQ process running in the container", [this] {
                return host_importer_->docker().is_process_running(importer_rabbitmq, "rabbitmq", localhost_);
            });
            views::cached_view<std::string>("RabbitMQ Queues", [this] {
                return host_importer_->docker().execute_command(
                    std::format("docker exec {} rabbitmqctl list_queues", importer_rabbitmq),
                    localhost_);
            }, [](std::string const &output) {
                ImGui::Text("%s", output.c_str());
            });
        }
    }
    
private:
    hosting::ssh::host::ptr host_importer_;
    hosting::local::host &localhost_;
};
