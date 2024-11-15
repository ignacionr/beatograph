#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <format>
#include <thread>
#include "../hosting/host.hpp"
#include "../hosting/screen.hpp"

struct cluster_report {
    cluster_report(hosting::local::host &localhost) : localhost_{localhost} {
        for (auto &host : hosts_) {
            host->resolve_from_ssh_conf(localhost_);
        }
    }

    ~cluster_report() {
        quit_ = true;
    }

    void render() {
        for (auto &host : hosts_) {
            host_screen_.render(host, localhost_);
        }
    }
    
private:
    std::array<hosting::ssh::host::ptr, 3> hosts_ {
        hosting::ssh::host::by_name("arangodb1"), 
        hosting::ssh::host::by_name("arangodb2"), 
        hosting::ssh::host::by_name("arangodb3")};
    hosting::ssh::screen host_screen_;
    hosting::local::host &localhost_;
    std::atomic<bool> quit_{false};
    std::unique_ptr<std::jthread> refresh_thread;
};
