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
private:
    host_ptr_t host_;
    std::atomic<std::shared_ptr<nlohmann::json>> docker_ps_;
};
