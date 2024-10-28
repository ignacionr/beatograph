#pragma once

#include <filesystem>
#include <format>
#include <memory>
#include <string>

#include <imgui.h>
#include "importer_report.hpp"
#include "../host/host.hpp"
#include "../host/screen.hpp"
#include "../views/assertion.hpp"
#include "../host/local_mapping.hpp"

extern "C"
{
#include <tinyfiledialogs.h>
}

struct dataoffering_screen
{
    static constexpr auto zookeeper_name = "storm-zookeeper";
    static constexpr auto nimbus_name = "storm-nimbus";
    static constexpr auto supervisor_name = "storm-supervisor";
    static constexpr auto ui_name = "storm-ui";
    static constexpr auto topology_name = "betmavrik-topology";
    static constexpr auto topology_class = "com.betmavrik.storm.TopologyMain";

    dataoffering_screen(host_local &localhost) : importer{localhost}, localhost_{localhost} {}
    void render()
    {
        ImGui::BeginChild("Data Offering");
        importer.render();
        if (ImGui::CollapsingHeader("Storm Topology Status", ImGuiTreeNodeFlags_DefaultOpen))
        {
            views::Assertion("storm1 docker is running a zookeeper", [this]
                             { return host::by_name("storm1")->docker().is_container_running(zookeeper_name, localhost_); });
            views::Assertion("the zookeeper process is running", [this]
                             { return host::by_name("storm1")->docker().is_process_running(zookeeper_name, "/opt/java/openjdk/bin/java -Dzookeeper.log.dir=/logs", localhost_); });
            views::Assertion("zookeeper responds to the client", [this]
                             {
                auto const result = host::by_name("storm1")->docker().execute_command(
                    std::format("docker exec {} zkCli.sh -server localhost:2181", zookeeper_name), localhost_);
                if (result.find("(CONNECTED) 0") == std::string::npos) {
                    throw std::runtime_error(std::format("Error: expected 'ZooKeeper -', got '{}'", result));
                }
                return true; });
            views::Assertion("storm1 docker is running a storm supervisor", [this]
                             { return host::by_name("storm1")->docker().is_container_running(supervisor_name, localhost_); });
            views::Assertion("storm1 docker is running a storm nimbus", [this]
                             { return host::by_name("storm1")->docker().is_container_running(nimbus_name, localhost_); });
            views::Assertion("storm1 docker is running a storm UI", [this]
                             { return host::by_name("storm1")->docker().is_container_running(ui_name, localhost_); });
            if (storm_ui_mapping_)
            {
                auto const url{std::format("http://localhost:{}", storm_ui_mapping_->local_port())};
                ImGui::Text("Storm UI is available at %s", url.c_str());
                if (ImGui::Button("Open Storm UI"))
                {
                    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOW);
                }
                if (ImGui::Button("Unmap Storm UI"))
                {
                    storm_ui_mapping_.reset();
                }
            }
            else
            {
                if (ImGui::Button("Map Storm UI"))
                {
                    storm_ui_mapping_ = std::make_unique<host_local_mapping>(8080, "storm1", localhost_);
                }
            }
            views::cached_view<std::string>("Storm Topology", [this]
                                            {
                auto cmd = std::format("docker exec {} storm list", nimbus_name);
                return host::by_name("storm1")->docker().execute_command(cmd, localhost_); }, [](std::string const &output)
                                            { ImGui::Text("%s", output.c_str()); });
            if (!installing_topology_ && ImGui::Button("Install Storm Topology"))
            {
                    std::thread([this]
                                {
                        try
                        {
                            installing_topology_ = true;
                            // upload the file to storm1 from ignacio-bench
                            // from the path /home/ubuntu/arangodb-infra/storm-topology/target/storm-topology-1.0-SNAPSHOT.jar
                            auto storm1{host::by_name("storm1")};
                            auto const filename{"storm-topology-1.0-SNAPSHOT.jar"};
                            auto on_storm1 = std::format("/tmp/{}", filename);
                            auto on_nimbus = on_storm1;
                            topology_install_result_ = "Uploading file...";

                            std::string cmd_upload_from_ignacio_bench = std::format(
                                "scp /home/ubuntu/arangodb-infra/storm-topology/target/{} storm1:/tmp/{}", filename, filename);
                            topology_install_result_ = host::by_name("ignacio-bench")->docker().execute_command(cmd_upload_from_ignacio_bench, localhost_, false);

                            // now upload it into the nimbus container
                            topology_install_result_ += "\n\nCopying file to nimbus...";
                            storm1->docker().copy_to_container(on_storm1,
                                                               nimbus_name,
                                                               on_nimbus,
                                                               localhost_);
                            topology_install_result_ = "Installing topology...";
                            auto cmd = std::format("docker exec {} storm jar {} {} {}", nimbus_name, on_nimbus, topology_class, topology_name);
                            topology_install_result_ = host::by_name("storm1")->docker().execute_command(cmd, localhost_);
                        }
                        catch (std::exception const &e)
                        {
                            topology_install_result_ = e.what();
                            installing_topology_ = false;
                        }
                        installing_topology_ = false; })
                        .detach();
                }
            if (!topology_install_result_.empty())
                ImGui::Text(topology_install_result_.c_str());
            views::Assertion("Storm Topology can resolve rabbitmq", [this]
                             {
                                // verify that the container running nimbus can resolve its way into ignacio-bench host, using curl
                                auto const cmd = std::format("docker exec {} curl http://rabbitmq", nimbus_name);
                                auto const result = host::by_name("storm1")->docker().execute_command(cmd, localhost_);
                                // if curl says it can't resolve the host, it means we can't connect
                                if (result.find("curl: (6) Could not resolve host") != std::string::npos)
                                {
                                    throw std::runtime_error("Error: storm topology can't resolve rabbitmq");
                                }
                                return true;
                             });
            ImGui::Text("* Hadoop Status");
            ImGui::Text("* Hbase Status");
            ImGui::EndChild();
        }
    }

    private:
        importer_report importer;
        host_local & localhost_;
        host_screen host_screen_;
        std::unique_ptr<host_local_mapping> storm_ui_mapping_;
        std::string topology_install_result_;
        bool installing_topology_{false};
};
