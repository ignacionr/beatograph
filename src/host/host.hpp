#pragma once
#include <string>
#include <map>
#include <memory>
#include <atomic>
#include <format>
#include "host_local.hpp"
#include "local_mapping.hpp"
#include "../metrics/from_url.hpp"
#include "../docker/host.hpp"

struct host {
    using properties_t = std::map<std::string, std::string>;
    using ptr = std::shared_ptr<host>;

    using hosts_by_name_t = std::map<std::string, std::weak_ptr<host>>;
    static ptr by_name(std::string const &name) 
    {
        auto it = hosts_().find(name);
        if (it != hosts_().end() && !it->second.expired()) {
            return it->second.lock();
        }
        return hosts_().emplace(
            name, 
            std::shared_ptr<host>(new host{name})).first->second.lock();
    }
    static void destroy_all() {
        for (auto &h : hosts_()) {
            if (!h.second.expired()) {
                h.second.reset();
            }
        }
    }
private:
    host(std::string_view name) : name_{name} {}
public:

    void resolve_from_ssh_conf(host_local &host_local) 
    {
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

    auto metrics() {
        return metrics_.load();
    }

    void fetch_metrics(host_local &localhost) {
        if (!nodeexporter_mapping_.load()) {
            nodeexporter_mapping_.store(std::make_shared<host_local_mapping>(9100, name_, localhost));
        }
        auto url = std::format("http://localhost:{}/metrics", nodeexporter_mapping_.load()->local_port());
        auto metrics = metrics_from_url::fetch(url);
        metrics_.store(std::make_shared<metrics_model>(std::move(metrics)));
    }

    auto &docker() 
    {
        if (!docker_host_) {
            docker_host_ = std::make_unique<docker_host>(name_);
        }
        return *docker_host_;
    }

private:

    static hosts_by_name_t &hosts_() {
        static hosts_by_name_t hosts;
        return hosts;
    }

    std::string name_;
    std::atomic<std::shared_ptr<properties_t>> properties_ = std::make_shared<properties_t>();
    std::atomic<std::shared_ptr<host_local_mapping>> nodeexporter_mapping_;
    std::atomic<std::shared_ptr<metrics_model>> metrics_;
    std::unique_ptr<docker_host> docker_host_;
};
