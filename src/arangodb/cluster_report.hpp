#pragma once
#include <array>
#include <atomic>
#include <thread>
#include "../host/host.hpp"
#include "../host/screen.hpp"

struct cluster_report {
    cluster_report(host_local &host_local) : host_local_{host_local} {
        for (auto &host : hosts_) {
            host.resolve_from_ssh_conf(host_local_);
        }
        refresh_thread = std::jthread([this] {
            while (!quit_) {
                for (auto &host : hosts_) {
                    try {
                        host.fetch_metrics(host_local_);
                    }
                    catch(std::exception const &e) {
                        std::cerr << "Error: " << e.what() << std::endl;
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(15));
            }
        });
    }
    ~cluster_report() {
        quit_ = true;
    }
    void render() {
        for (auto &host : hosts_) {
            ImGui::BeginChild("cluster-node-data");
            host_screen_.render(host);
            ImGui::EndChild();
        }
    }
private:
    std::array<host, 3> hosts_ {host("arangodb1"), host("arangodb2"), host("arangodb3")};
    host_screen host_screen_;
    host_local &host_local_;
    std::atomic<bool> quit_{false};
    std::jthread refresh_thread;
};
