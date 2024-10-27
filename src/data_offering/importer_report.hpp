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
            views::Assertion("Is the importer-odds-java-container-1 running?", [this] {
                auto ps = host_importer_->docker().ps();
                if (!ps) {
                    return false;
                }
                auto const &array {ps->get<nlohmann::json::array_t>()};
                return std::any_of(array.begin(), array.end(), [](auto const &container) {
                    return container.contains("Names") && container["Names"].get<std::string>().find("importer-odds-java-container-1") != std::string::npos;
                });
            });
            views::Assertion("Is the Java process running in the importer-odds-java-container-1 container?", [this] {
                return host_importer_->docker().is_process_running("importer-odds-java-container-1", "java", localhost_);
            });
        }
    }
    
private:
    host::ptr host_importer_ {host::by_name("ignacio-bench")};
    docker_screen docker_screen_;
    host_screen host_screen_;
    host_local &localhost_;
};
