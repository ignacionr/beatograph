#pragma once
#include <string>
#include <map>
#include <memory>
#include <atomic>
#include "host_local.hpp"

struct host {
    using properties_t = std::map<std::string, std::string>;
    host(std::string_view name) : name_(name) {}
    void resolve_from_ssh_conf(host_local &host_local) {
        std::string all_lines = host_local.execute_command(("ssh -G " + name_).c_str());
        std::string line;
        std::istringstream iss(all_lines);
        auto properties = std::make_shared<properties_t>();
        while (std::getline(iss, line)) {
            auto pos = line.find(' ');
            if (pos != std::string::npos) {
                properties->emplace(line.substr(0, pos), line.substr(pos + 1));
            }
        }
        properties_.store(std::move(properties));
    }
    std::string const &name() const { return name_; }
    properties_t const &properties() const {
        auto p = properties_.load();
        return *p;
    }
private:
    std::string name_;
    std::atomic<std::shared_ptr<properties_t>> properties_ = std::make_shared<properties_t>();
};
