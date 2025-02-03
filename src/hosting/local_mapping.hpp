#pragma once

#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <stdexcept>
#include "host_local.hpp"

#pragma comment(lib, "ws2_32.lib")

namespace hosting::local
{
    struct mapping
    {
        mapping(unsigned short port, const std::string &hostname, std::shared_ptr<host> localhost)
            : mapped_port_{port}, hostname_{hostname}, localhost_{localhost}
        {
            std::cerr << "Starting port mapping for " << hostname_ << " port " << mapped_port_ << std::endl;
            // Find an available port
            local_port_ = 0;
            {
                static unsigned short watermark_{10000};
                static std::mutex port_mutex;
                std::lock_guard<std::mutex> lock(port_mutex);

                for (unsigned short port_candidate = watermark_; port_candidate < 65535; ++port_candidate)
                {
                    if (!localhost_->IsPortInUse(port_candidate))
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
                watermark_ = local_port_ + 1;
            }
            // Map the port
            std::string command = std::format("ssh -o ConnectTimeout=10 -N -L {0}:localhost:{1} {2}", local_port_, mapped_port_, hostname_);
            process_ = localhost->run(command.c_str());
        }
        ~mapping()
        {
            std::cerr << "Stopping port mapping for " << hostname_ << " port " << mapped_port_ << std::endl;
            process_->sendQuitSignal(CTRL_C_EVENT);
            process_->wait(400);
            process_->sendQuitSignal(CTRL_BREAK_EVENT);
            process_->wait(400);
            process_->stop();
            process_->wait(1000);
        }
        unsigned short mapped_port() const { return mapped_port_; }
        unsigned short local_port() const { return local_port_; }
        auto localhost() const { return localhost_; }
        auto hostname() const { return hostname_; }

    private:
        unsigned short mapped_port_;
        unsigned short local_port_;
        std::string hostname_;
        std::shared_ptr<host> localhost_;
        std::unique_ptr<running_process> process_;
    };
}
