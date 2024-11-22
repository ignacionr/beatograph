#pragma once

#include <chrono>
#include <format>
#include <string>

#include "../hosting/host_local.hpp"

namespace report
{
    struct host
    {
        host(hosting::local::host &host) : host_(host) {}
        std::string operator()() {
            std::string result;
            // tell the current time, in the local timezone
            auto const now {std::chrono::system_clock::now()};
            result += std::format("The current time UTC is {:%H %M}.\n", now);
            auto const ip_and_geo {host_.get_my_ip_and_geolocation()};
            result += std::format("You are connecting from {}, {}.\n",
                ip_and_geo.at("city").get_ref<const std::string&>(), 
                ip_and_geo.at("region").get_ref<const std::string&>());
            return result;
        }
    private:
        hosting::local::host &host_;
    };
}