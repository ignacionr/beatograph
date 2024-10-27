#pragma once

#include <algorithm>
#include <atomic>
#include <memory>
#include <ranges>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include "../host/host.hpp"
#include "../host/screen.hpp"
#include "../docker/host.hpp"
#include "../docker/screen.hpp"
#include "../views/assertion.hpp"

struct importer_report {
    static auto constexpr host_importer_name = "ignacio-bench";
    static auto constexpr importer_container_name = "importing-odds-java-producer-1";
    static auto constexpr importer_rabbitmq = "importing-odds-rabbitmq-1";

    importer_report(host_local &localhost): 
        localhost_{localhost} 
    {
        auto t = std::thread([this] {
            host_importer_->resolve_from_ssh_conf(localhost_);
            try {
                host_importer_->fetch_metrics(localhost_);
                host_importer_->docker().fetch_ps(localhost_);
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
            if (ImGui::CollapsingHeader("Importer Docker Host Status")) {
                host_screen_.render(host_importer_, localhost_);
            }
            views::Assertion("Importer Container running", [this] {
                auto ps = host_importer_->docker().ps();
                if (!ps) {
                    return false;
                }
                auto const &array {ps->get<nlohmann::json::array_t>()};
                return std::any_of(array.begin(), array.end(), [](auto const &container) {
                    if (container.contains("Names")) {
                        auto names = container["Names"].get<std::string>();
                        return names.find(importer_container_name) != std::string::npos;
                    }
                    return false;
                });
            });
            views::Assertion("Java process running in the importer container", [this] {
                return host_importer_->docker().is_process_running(importer_container_name, "java", localhost_);
            });
            views::Assertion("RabbitMQ container running", [this] {
                auto ps = host_importer_->docker().ps();
                if (!ps) {
                    return false;
                }
                auto const &array {ps->get<nlohmann::json::array_t>()};
                return std::any_of(array.begin(), array.end(), [](auto const &container) {
                    if (container.contains("Names")) {
                        auto names = container["Names"].get<std::string>();
                        return names.find(importer_rabbitmq) != std::string::npos;
                    }
                    return false;
                });
            });
            views::Assertion("RabbitMQ process running in the container", [this] {
                return host_importer_->docker().is_process_running(importer_rabbitmq, "rabbitmq", localhost_);
            });
        }
    }
    
private:
    host::ptr host_importer_ {host::by_name(host_importer_name)};
    docker_screen docker_screen_;
    host_screen host_screen_;
    host_local &localhost_;
};
