#pragma once

#include <iostream>
#include <cstdio> // for popen and pclose
#include <memory> // for smart pointers
#include <string> // for strings

struct host_local
{
    std::string execute_command(const char *command)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);

        if (!pipe)
        {
            throw std::runtime_error("popen() failed!");
        }

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        {
            result += buffer.data();
        }

        return result;
    }
};
