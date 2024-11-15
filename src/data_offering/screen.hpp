#pragma once

#include <array>
#include <filesystem>
#include <format>
#include <memory>
#include <string>

#include <imgui.h>
#include <cppgpt/cppgpt.hpp>
#include "importer_report.hpp"
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
        static constexpr auto hadoop_zookeeper_name{"hadoop-zookeeper-1"};

        screen(hosting::local::host &localhost, std::string const &groq_api_key, std::string_view api_client_key)
            : importer{localhost}, localhost_{localhost}, groq_api_key_{groq_api_key}
        {
            client = std::make_unique<api_client>(api_client_key);
        }

        void render()
        {
            if (ImGui::BeginChild("Data Offering"))
            {
                importer.render();
                if (ImGui::CollapsingHeader("Storm Topology Status"))
                {
                    views::Assertion("storm1 docker is running a zookeeper", [this]
                                     { return hosting::ssh::host::by_name("storm1")->docker().is_container_running(zookeeper_name, localhost_); });
                    views::Assertion("the zookeeper process is running", [this]
                                     { return hosting::ssh::host::by_name("storm1")->docker().is_process_running(zookeeper_name, "/opt/java/openjdk/bin/java -Dzookeeper.log.dir=/logs", localhost_); });
                    views::Assertion("zookeeper responds to the client", [this]
                                     {
                    auto const result = hosting::ssh::host::by_name("storm1")->docker().execute_command(
                        std::format("docker exec {} zkCli.sh -server localhost:2181", zookeeper_name), localhost_);
                    if (result.find("(CONNECTED) 0") == std::string::npos) {
                        throw std::runtime_error(std::format("Error: expected 'ZooKeeper -', got '{}'", result));
                    }
                    return true; });
                    views::Assertion("storm1 docker is running a storm supervisor", [this]
                                     { return hosting::ssh::host::by_name("storm1")->docker().is_container_running(supervisor_name, localhost_); });
                    views::Assertion("storm1 docker is running a storm nimbus", [this]
                                     { return hosting::ssh::host::by_name("storm1")->docker().is_container_running(nimbus_name, localhost_); });
                    views::Assertion("storm1 docker is running a storm UI", [this]
                                     { return hosting::ssh::host::by_name("storm1")->docker().is_container_running(ui_name, localhost_); });
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
                            storm_ui_mapping_ = std::make_unique<hosting::local::mapping>(8080, "storm1", localhost_);
                        }
                    }
                    views::cached_view<std::string>("Storm Topology", [this]
                                                    {
                    auto cmd = std::format("docker exec {} storm list", nimbus_name);
                    return hosting::ssh::host::by_name("storm1")->docker().execute_command(cmd, localhost_); }, [](std::string const &output)
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
                                auto storm1{hosting::ssh::host::by_name("storm1")};
                                auto const filename{"storm-topology-1.0-SNAPSHOT.jar"};
                                auto on_storm1 = std::format("/tmp/{}", filename);
                                auto on_nimbus = on_storm1;
                                topology_install_result_ = "Uploading file...";

                                std::string cmd_upload_from_ignacio_bench = std::format(
                                    "scp /home/ubuntu/arangodb-infra/storm-topology/target/{} storm1:/tmp/{}", filename, filename);
                                topology_install_result_ = hosting::ssh::host::by_name("ignacio-bench")->docker().execute_command(cmd_upload_from_ignacio_bench, localhost_, false);

                                // now upload it into the nimbus container
                                topology_install_result_ += "\n\nCopying file to nimbus...";
                                storm1->docker().copy_to_container(on_storm1,
                                                                nimbus_name,
                                                                on_nimbus,
                                                                localhost_);
                                topology_install_result_ = "Installing topology...";
                                auto cmd = std::format("docker exec {} storm jar {} {} {}", nimbus_name, on_nimbus, topology_class, topology_name);
                                topology_install_result_ = hosting::ssh::host::by_name("storm1")->docker().execute_command(cmd, localhost_);
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
                                    auto const result = hosting::ssh::host::by_name("storm1")->docker().execute_command(cmd, localhost_);
                                    // if curl says it can't resolve the host, it means we can't connect
                                    if (result.find("curl: (6) Could not resolve host") != std::string::npos)
                                    {
                                        throw std::runtime_error("Error: storm topology can't resolve rabbitmq");
                                    }
                                    return true; });
                }

                if (ImGui::CollapsingHeader("Hadoop and Hbase Status"))
                {
                    ImGui::Text("* Hadoop Status");
                    ImGui::Text("* Hbase Status");
                    views::Assertion("Zookeeper is running on hadoop1", [this]
                                     { return hosting::ssh::host::by_name(hadoop_host)->docker().is_container_running(hadoop_zookeeper_name, localhost_); });
                    views::Assertion("Hadoop Resource Manager is running", [this]
                                     { return hosting::ssh::host::by_name(hadoop_host)->docker().is_container_running("hadoop-resourcemanager-1", localhost_); });
                    views::Assertion("Hadoop Node Manager is running", [this]
                                     { return hosting::ssh::host::by_name(hadoop_host)->docker().is_container_running("hadoop-nodemanager-1", localhost_); });
                    views::Assertion("Hadoop Name Node is running", [this]
                                     { return hosting::ssh::host::by_name(hadoop_host)->docker().is_container_running("hadoop-namenode-1", localhost_); });
                    views::Assertion("Hadoop Data Node is running", [this]
                                     { return hosting::ssh::host::by_name(hadoop_host)->docker().is_container_running("hadoop-datanode-1", localhost_); });
                    views::Assertion("Hbase Region Server is running", [this]
                                     { return hosting::ssh::host::by_name(hadoop_host)->docker().is_container_running("hadoop-hbase-regionserver-1", localhost_); });
                    views::Assertion("Hbase Master is running", [this]
                                     { return hosting::ssh::host::by_name(hadoop_host)->docker().is_container_running("hadoop-hbase-master-1", localhost_); });
                    auto retrieve_assessment_and_logs = [this](std::string const &service_type, std::vector<std::string> const &servers) -> std::array<std::string, 2>
                    {
                        std::string logs;
                        auto const per_log_limit{5000 / servers.size()};
                        for (auto const &server : servers)
                        {
                            auto server_logs = hosting::ssh::host::by_name(hadoop_host)->docker().logs(server, localhost_);
                            server_logs = server_logs.substr(std::max(static_cast<long>(server_logs.size() - per_log_limit), 0l));
                            logs += std::format("Service: {}\n{}\n\n", server, server_logs);
                        }
                        auto const prompt = std::format("You are a {} expert. Given the following logs, please assess the status of the service.\n\n{}", service_type, logs);
                        ignacionr::cppgpt gpt_{groq_api_key_, ignacionr::cppgpt::groq_base};
                        return {
                            gpt_.sendMessage(prompt, "user", "llama-3.2-90b-text-preview")["choices"][0]["message"]["content"].get<std::string>(),
                            logs};
                    };
                    auto show_assessment = [](std::array<std::string, 2> const &output)
                    {
                        ImGui::TextWrapped("%s", output[0].c_str());
                        if (ImGui::CollapsingHeader("Logs"))
                        {
                            if (ImGui::BeginChild("Logs", {ImGui::GetWindowWidth() - 20, 300}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar))
                            {
                                ImGui::TextWrapped("%s", output[1].c_str());
                            }
                            ImGui::EndChild();
                        }
                    };
                    views::cached_view<std::array<std::string, 2>>("Hadoop Assessment", [this, retrieve_assessment_and_logs]
                                                                   { return retrieve_assessment_and_logs("Hadoop",
                                                                                                         {"hadoop-resourcemanager-1", "hadoop-nodemanager-1", "hadoop-namenode-1", "hadoop-datanode-1"}); }, show_assessment);
                    views::cached_view<std::array<std::string, 2>>("Hbase Assessment", [this, retrieve_assessment_and_logs]
                                                                   { return retrieve_assessment_and_logs("Hbase",
                                                                                                         {"hadoop-hbase-regionserver-1", "hadoop-hbase-master-1"}); }, show_assessment);
                }

                if (ImGui::CollapsingHeader("Storm to Hadoop Data Transfer"))
                {
                    views::Assertion("Storm topology can reach the Hbase port", [this]
                                     {
                                        // verify that the container running nimbus can resolve its way into ignacio-bench host, using curl
                                        auto const cmd = std::format("docker exec {} curl http://host.docker.internal:16010", nimbus_name);
                                        auto const result = hosting::ssh::host::by_name("storm1")->docker().execute_command(cmd, localhost_);
                                        // if curl says it can't resolve the host, it means we can't connect
                                        if (result.find("curl: (6) Could not resolve host") != std::string::npos)
                                        {
                                            throw std::runtime_error(result);
                                        }
                                        return true; });
                    views::cached_view<std::string>("Checker", [this]() -> std::string
                                                    { return hosting::ssh::host::by_name("storm1")->docker().execute_command(
                                                          "docker logs --tail 1 storm-checker-1 2>&1",
                                                          localhost_); }, [](std::string const &output)
                                                    { ImGui::TextUnformatted(output.c_str()); });
                }

                if (ImGui::CollapsingHeader("Tools"))
                {
                    if (ImGui::Button("Code on Storm1"))
                    {
                        ShellExecuteA(nullptr, nullptr, "cmd.exe", "/c code --remote ssh-remote+storm1 /home/ubuntu/arangodb-infra/storm", nullptr, SW_SHOW);
                    }
                    else if (ImGui::SameLine(); ImGui::Button("Code on Hadoop1"))
                    {
                        ShellExecuteA(nullptr, nullptr, "cmd.exe", "/c code --remote ssh-remote+hadoop1 /home/ubuntu/repos/arangodb-infra/hadoop", nullptr, SW_SHOW);
                    }
                    else if (ImGui::SameLine(); ImGui::Button("Open Hbase Shell"))
                    {
                        ShellExecuteA(nullptr, nullptr, "cmd.exe", "/c ssh -t hadoop1 docker exec -it hadoop-hbase-master-1 hbase shell", nullptr, SW_SHOW);
                    }
                }
                if (ImGui::CollapsingHeader("API")) {
                    static char buff[100]{};
                    ImGui::InputText("Item ID", buff, sizeof(buff));
                    views::cached_view<std::string>("api", 
                        [this]() -> std::string {
                            return client->get_entity(std::stoll(buff));
                        },
                        [this](std::string result){
                            json_view.render(result);
                        });
                }

                ImGui::EndChild();
            }
        }

    private:
        importer_report importer;
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