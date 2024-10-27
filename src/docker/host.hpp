#pragma once

#include <format>
#include <nlohmann/json.hpp>

#include "../host/host_local.hpp"

template<typename host_ptr_t>
struct docker_host {
    docker_host(host_ptr_t host) : host_{host} {}

    std::string execute_command(std::string const &command, host_local &localhost) const {
        return localhost.execute_command(std::format("ssh {} sudo {}", host_->name(), command).c_str());
    }

    void fetch_ps(host_local &localhost) {
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

    void open_shell(std::string const &container_id, host_local &localhost) const {
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
        auto cmd = std::format("/c ssh -t {} sudo docker exec -it {} {}", host_->name(), container_id, shell);
        ShellExecuteA(nullptr, 
            "open", 
            "cmd.exe", 
            cmd.c_str(), 
            nullptr, 
            SW_SHOW);
    }

    void open_logs(std::string const &container_id) const {
        auto cmd = std::format("/c ssh {} sudo docker logs {}", host_->name(), container_id);
        ShellExecuteA(nullptr, 
            "open", 
            "cmd.exe", 
            cmd.c_str(), 
            nullptr, 
            SW_SHOW);
    }

    std::string all_processes(std::string const &container_id, host_local &localhost) const {
        return execute_command(std::format("docker exec {} ps aux", container_id), localhost);
    }

    bool is_process_running(std::string const &container_id, std::string const &process_name, host_local &localhost) const {
        auto processes = all_processes(container_id, localhost);
        return processes.find(process_name) != std::string::npos;
    }
private:
    host_ptr_t host_;
    std::atomic<std::shared_ptr<nlohmann::json>> docker_ps_;
};
