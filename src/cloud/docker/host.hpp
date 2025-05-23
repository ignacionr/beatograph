#pragma once

#include <format>
#include <nlohmann/json.hpp>

#include "../../hosting/host_local.hpp"
namespace docker {
struct host {
    host(std::string const &host_name) : host_name_{host_name} {}

    std::string execute_command(std::string_view command, std::shared_ptr<hosting::local::host> localhost, bool sudo = true) const {
        return localhost->ssh(std::format("{} {}", sudo ? "sudo" : "", command), host_name_);
    }

    std::string execute_command(std::string_view command, std::string_view container_id, std::shared_ptr<::hosting::local::host> localhost, bool sudo = true) const {
        auto cmd = std::format("docker exec {} {} {}", container_id, sudo ? "sudo" : "", command);
        cmd = localhost->resolve_environment(cmd);
        return localhost->ssh(cmd, host_name_);
    }

    void fetch_ps(std::shared_ptr<::hosting::local::host> localhost) {
        auto result = execute_command("docker ps -a --format json", localhost);
        // the result is not a json, but a succession of json lines
        // we need to add the missing commas to make it a valid json array
        std::string json_array = "[";
        for (auto &c : result) {
            if (c == '\n') {
                json_array += ",";
            }
            else {
                json_array += c;
            }
        }
        // remove the last comma
        if (json_array.size() > 1) json_array.pop_back();
        json_array += "]";
        
        docker_ps_.store(std::make_shared<nlohmann::json>(nlohmann::json::parse(json_array)));
    }

    std::shared_ptr<nlohmann::json> ps() const {
        return docker_ps_.load();
    }

    void open_shell(std::string const &container_id, std::shared_ptr<::hosting::local::host> localhost) const {
        auto ls_command = std::format("docker exec {} which bash zsh sh", container_id);
        auto ls_response = execute_command(ls_command, localhost);
        // use the first line of the response to determine the shell
        std::string shell;
        for (auto &c : ls_response) {
            if (c == '\n') {
                break;
            }
            shell += c;
        }
        if (shell.empty()) {
            return;
        }
        auto cmd = std::format("/c ssh -t {} sudo docker exec -it {} {}", host_name_, container_id, shell);
        ShellExecuteA(nullptr, 
            "open", 
            "cmd.exe", 
            cmd.c_str(), 
            nullptr, 
            SW_SHOW);
    }

    void open_logs(std::string const &container_id) const {
        auto cmd = std::format("/c ssh {} sudo docker logs -f {} && pause", host_name_, container_id);
        ShellExecuteA(nullptr, 
            "open", 
            "cmd.exe", 
            cmd.c_str(), 
            nullptr, 
            SW_SHOW);
    }

    std::string logs(std::string const &container_id, std::shared_ptr<::hosting::local::host> localhost) const {
        return execute_command(std::format("docker logs {}", container_id), localhost);
    }

    std::string all_processes(std::string const &container_id, std::shared_ptr<::hosting::local::host> localhost) const {
        return execute_command(std::format("docker exec {} ps aux", container_id), localhost);
    }

    void copy_to_container(std::string const &file_path, std::string const &container_id, std::string const &destination, std::shared_ptr<::hosting::local::host> localhost) const {
        auto cmd = std::format("docker cp {} {}:{}", file_path, container_id, destination);
        execute_command(cmd, localhost);
    }

    bool is_container_running(std::string const &container_id_or_name, std::shared_ptr<::hosting::local::host> localhost) {
        auto ps = this->ps();
        if (!ps) {
            fetch_ps(localhost);
            ps = this->ps();
            if (!ps) {
                throw std::runtime_error("Error: could not fetch ps");
            }
        }
        if (!ps->is_array()) {
            throw std::runtime_error(std::format("Error: expected array, got {}", ps->dump()));
        }
        auto const &array {ps->get<nlohmann::json::array_t>()};
        return std::any_of(array.begin(), array.end(), [&container_id_or_name](auto const &container) {
            bool found{false};
            if (container.contains("Names")) {
                auto names = container["Names"].template get<std::string>();
                found = names == container_id_or_name;
            }
            if (!found && container.contains("ID")) {
                auto id = container["ID"].template get<std::string>();
                found = id.find(container_id_or_name) != std::string::npos;
            }
            if (found) {
                if (container.contains("State")) {
                    auto state = container["State"].template get<std::string>();
                    found = state == "running";
                }
                else {
                    found = false;
                }
            }
            return found;
        });
    }

    bool is_process_running(std::string const &container_id, std::string const &process_name, std::shared_ptr<::hosting::local::host> localhost) const {
        auto processes = all_processes(container_id, localhost);
        return processes.find(process_name) != std::string::npos;
    }
private:
    std::string host_name_;
    std::atomic<std::shared_ptr<nlohmann::json>> docker_ps_;
};
}