#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <format>
#include <thread>
#include "../host/host.hpp"
#include "../host/screen.hpp"

struct cluster_report {
    cluster_report(host_local &host_local) : host_local_{host_local} {
        for (auto &host : hosts_) {
            host->resolve_from_ssh_conf(host_local_);
        }
    }

    ~cluster_report() {
        quit_ = true;
    }

    void render() {
        for (auto &host : hosts_) {
            host_screen_.render(host, host_local_);
        }
    }
    
private:
    std::array<host::ptr, 3> hosts_ {host::by_name("arangodb1"), host::by_name("arangodb2"), host::by_name("arangodb3")};
    host_screen host_screen_;
    host_local &host_local_;
    std::atomic<bool> quit_{false};
    std::unique_ptr<std::jthread> refresh_thread;
};
