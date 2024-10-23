#pragma once

#include <format>
#include <memory>
#include <string>
#include <stdexcept>
#include "host_local.hpp"

#pragma comment(lib, "ws2_32.lib")

struct host_local_mapping
{
    host_local_mapping(unsigned short port, const std::string &hostname, host_local &localhost)
        : mapped_port_{port}, hostname_{hostname}, localhost_{localhost}
    {
        // Find an available port
        local_port_ = 0;
        for (unsigned short port_candidate = 10000; port_candidate < 65535; ++port_candidate)
        {
            if (!localhost_.IsPortInUse(port_candidate))
            {
                local_port_ = port_candidate;
                break;
            }
        }
        // If no port is available, throw an exception
        if (local_port_ == 0)
        {
            throw std::runtime_error("No available ports found!");
        }
        // Map the port
        std::string command = std::format("ssh -L {0}:localhost:{1} {2} sleep infinity", local_port_, mapped_port_, hostname_);
        process_ = localhost.run(command.c_str());
    }
    ~host_local_mapping()
    {
        process_->sendQuitSignal();
        process_->wait(1000);
        process_->stop();
    }
private:
    unsigned short mapped_port_;
    unsigned short local_port_;
    std::string hostname_;
    host_local &localhost_;
    std::unique_ptr<host_local::running_process> process_;
};
