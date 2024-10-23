#pragma once

#include <format>
#include <nlohmann/json.hpp>

#include "../host/host_local.hpp"
#include "../host/host.hpp"

struct docker_host {
    docker_host(host& host) : host_{host} {}
    std::string execute_command(std::string const &command, host_local &localhost) const {
        return localhost.execute_command(std::format("ssh {} {}", host_.name(), command).c_str());
    }
    nlohmann::json ps(host_local &localhost) const {
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
        json_array.pop_back();
        json_array += "]";
        
        return nlohmann::json::parse(json_array);
    }
private:
    host& host_;
};
