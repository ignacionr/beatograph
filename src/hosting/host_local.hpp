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

#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include "ssh_execute.hpp"
#include "local_process.hpp"

namespace hosting::local
{
    struct host
    {
        host() = default;
        host(const host &) = delete;

        std::string get_env_variable(std::string const &key)
        {
            char *val = nullptr;
            size_t len = 0;
            if (_dupenv_s(&val, &len, key.c_str()) || val == nullptr)
            {
                return std::string{};
            }
            return std::string{val, len - 1}; // len returns the count of all copied bytes, including the terminator
        }

        void set_env_variable(std::string const &key, std::string const &value)
        {
            _putenv_s(key.c_str(), value.c_str());
        }

        std::string resolve_environment(std::string source) {
            // perform environment variable substitutions with ${VAR} syntax
            for (size_t pos = 0; (pos = source.find("${", pos)) != std::string::npos;)
            {
                size_t end = source.find('}', pos);
                if (end == std::string::npos)
                {
                    throw std::runtime_error("Invalid environment variable substitution syntax.");
                }
                std::string_view var_name {source.data() + pos + 2, end - pos - 2};
                source.replace(pos, end - pos + 1, get_env_variable(std::string{var_name}));
            }
            return source;
        }

        auto run(std::string_view command, bool include_stderr = true)
        {
            return std::make_unique<running_process>(command, [this](std::string_view key)
                                                      { return resolve_environment(std::string{key}); },
                                                      include_stderr);
        }

        void execute_command(std::string_view command, auto sink, bool include_stderr = true)
        {
            auto proc{run(command, include_stderr)};
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

        std::string execute_command(std::string_view command, bool include_stderr = true)
        {
            std::string result;
            execute_command(command, [&result](std::string_view line)
                            { result += line; }, include_stderr);
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

        static size_t WriteString(void *contents, size_t size, size_t nmemb, std::string *s) {
            s->append((char *)contents, size * nmemb);
            return size * nmemb;
        };

        nlohmann::json::object_t get_my_ip_and_geolocation() const
        {
            std::string location;
            CURL *curl;
            CURLcode res;
            curl = curl_easy_init();
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, "https://ipinfo.io");
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteString);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &location);
                res = curl_easy_perform(curl);
                if (res != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    throw std::runtime_error("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
                }
                curl_easy_cleanup(curl);
            }
            return nlohmann::json::parse(location);
        }

    private:
        std::string hostname;
        std::unordered_map<std::string, std::unique_ptr<ssh_execute>> sessions;
    };
}