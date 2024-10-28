#pragma once

#include <format>
#include <memory>
#include <string>

#include <imgui.h>
#include "importer_report.hpp"
#include "../host/host.hpp"
#include "../host/screen.hpp"
#include "../views/assertion.hpp"
#include "../host/local_mapping.hpp"

struct dataoffering_screen {
    static constexpr auto zookeeper_name = "ubuntu-zookeeper-1";
    dataoffering_screen(host_local &localhost) : importer{localhost}, localhost_{localhost} {}
    void render() {
        ImGui::BeginChild("Data Offering");
        importer.render();
        if (ImGui::CollapsingHeader("Storm Topology Status", ImGuiTreeNodeFlags_DefaultOpen)) {
            views::Assertion("storm1 docker is running a zookeeper", [this] {
                return host::by_name("storm1")->docker().is_container_running(zookeeper_name, localhost_);
            });
            views::Assertion("the zookeeper process is running", [this] {
                return host::by_name("storm1")->docker().is_process_running(zookeeper_name, "/opt/java/openjdk/bin/java -Dzookeeper.log.dir=/logs", localhost_);
            });
            views::Assertion("zookeeper responds to the client", [this] {
                auto const result = host::by_name("storm1")->docker().execute_command(
                    std::format("docker exec {} zkCli.sh -server localhost:2181", zookeeper_name), localhost_);
                if (result.find("(CONNECTED) 0") == std::string::npos) {
                    throw std::runtime_error(std::format("Error: expected 'ZooKeeper -', got '{}'", result));
                }
                return true;
            });
            views::Assertion("storm1 docker is running a storm supervisor", [this] {
                return host::by_name("storm1")->docker().is_container_running("supervisor-1", localhost_);
            });
            views::Assertion("storm1 docker is running a storm nimbus", [this] {
                return host::by_name("storm1")->docker().is_container_running("nimbus-1", localhost_);
            });
            views::Assertion("storm1 docker is running a storm UI", [this] {
                return host::by_name("storm1")->docker().is_container_running("ui-1", localhost_);
            });
            if (storm_ui_mapping_) {
                auto const url {std::format("http://localhost:{}", storm_ui_mapping_->local_port())};
                ImGui::Text("Storm UI is available at %s", url.c_str());
                if (ImGui::Button("Open Storm UI")) {
                    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOW);
                }
                if (ImGui::Button("Unmap Storm UI")) {
                    storm_ui_mapping_.reset();
                }
            }
            else {
                if (ImGui::Button("Map Storm UI")) {
                    storm_ui_mapping_ = std::make_unique<host_local_mapping>(8080, "storm1", localhost_);
                }
            }
        }
        ImGui::Text("* Hadoop Status");
        ImGui::Text("* Hbase Status");
        ImGui::EndChild();
    }
private:
    importer_report importer;
    host_local &localhost_;
    host_screen host_screen_;
    std::unique_ptr<host_local_mapping> storm_ui_mapping_;
};
