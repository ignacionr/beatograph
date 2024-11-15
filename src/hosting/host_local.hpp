#pragma once

#include <cstdio> // for popen and pclose
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <string_view>
#include <vector>

#include "ssh_execute.hpp"
#include "local_process.hpp"

namespace hosting::local
{
    struct host
    {

        auto run(const char *command)
        {
            return std::make_unique<running_process>(command);
        }

        void execute_command(const char *command, auto sink)
        {
            auto proc{run(command)};
            proc->read_all(sink);
        }

        std::string ssh(std::string_view command, std::string_view host_name, unsigned int timeout_seconds = 5)
        {
            std::string key{host_name};
            if (sessions.find(key) == sessions.end())
            {
                sessions[key] = std::make_unique<ssh_execute>(std::string{host_name}, timeout_seconds);
            }
            return sessions.at(key)->execute_command(std::string{command});
        }

        void recycle_session(std::string_view host_name)
        {
            std::string key{host_name};
            sessions.erase(key);
        }

        bool has_session(std::string_view host_name)
        {
            std::string key{host_name};
            return sessions.find(key) != sessions.end();
        }

        std::string execute_command(const char *command)
        {
            std::string result;
            execute_command(command, [&result](std::string_view line)
                            { result += line; });
            return result;
        }

        bool IsPortInUse(unsigned short port) const
        {
            WSADATA wsaData;
            SOCKET TestSocket = INVALID_SOCKET;
            struct sockaddr_in service;

            // Initialize Winsock
            int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (iResult != 0)
            {
                return false; // Winsock init failed, assume port is not in use
            }

            // Create a SOCKET for connecting to the server
            TestSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (TestSocket == INVALID_SOCKET)
            {
                WSACleanup();
                return false; // Socket creation failed, assume port is not in use
            }

            // Set up the sockaddr_in structure
            service.sin_family = AF_INET;
            if (inet_pton(AF_INET, "127.0.0.1", &service.sin_addr) <= 0)
            {
                closesocket(TestSocket);
                WSACleanup();
                return false; // Address conversion failed, assume port is not in use
            }
            service.sin_port = htons(port); // Convert port number

            // Attempt to connect to the port
            iResult = connect(TestSocket, (SOCKADDR *)&service, sizeof(service));
            if (iResult == SOCKET_ERROR)
            {
                // If the error is WSAECONNREFUSED or WSAETIMEDOUT, port is not in use
                int errCode = WSAGetLastError();
                if (errCode == WSAECONNREFUSED || errCode == WSAETIMEDOUT)
                {
                    closesocket(TestSocket);
                    WSACleanup();
                    return false; // Port is not in use
                }
            }
            else
            {
                // Connection succeeded, meaning the port is in use
                closesocket(TestSocket);
                WSACleanup();
                return true; // Port is in use
            }

            // Cleanup
            closesocket(TestSocket);
            WSACleanup();
            return false; // Port is not in use
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
        std::unordered_map<std::string, std::unique_ptr<ssh_execute>> sessions;
    };
}