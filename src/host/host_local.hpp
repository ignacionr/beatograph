#pragma once

#include <iostream>
#include <cstdio> // for popen and pclose
#include <stdio.h>
#include <memory> // for smart pointers
#include <string> // for strings

struct host_local
{
    std::string execute_command(const char *command)
    {
        std::array<char, 128> buffer;
        std::string result;
        #ifdef _WIN32
                std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(command, "r"), _pclose);
        #else
                std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
        #endif
        if (!pipe)
        {
            throw std::runtime_error("popen() failed!");
        }

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
        {
            result += buffer.data();
        }

        return result;
    }

    std::string HostName()
    {
        if (hostname.empty())
        {
            hostname = execute_command("hostname");
        }
        return hostname;
    }

private:
    std::string hostname;
};
