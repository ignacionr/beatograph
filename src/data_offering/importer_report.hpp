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

struct importer_report {
    importer_report(host_local &localhost): 
        docker_host_{host_importer_},
        localhost_{localhost} 
    {
        auto t = std::thread([this] {
            host_importer_->resolve_from_ssh_conf(localhost_);
            try {
                host_importer_->fetch_metrics(localhost_);
                docker_host_.fetch_ps(localhost_);
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
                host_screen_.render(host_importer_);
            }
        }
    }
private:
    host::ptr host_importer_ {host::by_name("ignacio-bench")};
    docker_host<host::ptr> docker_host_;
    host_screen host_screen_;
    host_local &localhost_;
};
