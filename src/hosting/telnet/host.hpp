#pragma once

#include <format>
#include <functional>
#include <stdexcept>
#include <thread>
#include "options.hpp"

namespace hosting::telnet
{
    class host
    {
    public:
        using handler_t = std::function<std::string(std::string_view)>;
        void run(std::function<bool()> quit, handler_t handler)
        {
            // bind to telnet port
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            {
                throw std::runtime_error("WSAStartup failed");
            }
            socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (socket_ == INVALID_SOCKET)
            {
                throw std::runtime_error("socket failed");
            }
            sockaddr_in service;
            service.sin_family = AF_INET;
            service.sin_addr.s_addr = INADDR_ANY;
            service.sin_port = htons(23);
            if (bind(socket_, (SOCKADDR *)&service, sizeof(service)) == SOCKET_ERROR)
            {
                // obtain the error description
                int error = WSAGetLastError();
                char buffer[256];
                FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, buffer, sizeof(buffer), NULL);
                closesocket(socket_);
                throw std::runtime_error(std::format("bind failed: {}", buffer));
            }
            thread_ = std::jthread([this, quit, handler]()
                                   {
                listen(socket_, 5);
                while (!quit())
                {
                    fd_set readfds;
                    FD_ZERO(&readfds);
                    FD_SET(socket_, &readfds);
                    timeval timeout;
                    timeout.tv_sec = 2; // 2 seconds timeout
                    timeout.tv_usec = 0;
                    int result = select(0, &readfds, NULL, NULL, &timeout);
                    if (result == SOCKET_ERROR || result == 0) {
                        continue; // timeout or error, continue to next iteration
                    }
                    SOCKET acceptSocket = accept(socket_, NULL, NULL);
                    if (acceptSocket != INVALID_SOCKET)
                    {
                    std::thread([acceptSocket, handler]() {
                        // first negotiate telnet options
                        option_group options;
                        options.add(std::make_unique<option_echo>());
                        options.add(std::make_unique<option_binary>());
                        options.add(std::make_unique<option_terminal_type>());
                        auto const initiate {options.initiate()};
                        send(acceptSocket, initiate.c_str(), static_cast<int>(initiate.size()), 0);
                        std::string line; // Buffer for incoming data
                        std::string echo;
                        option_terminal_type& terminal_type = *static_cast<option_terminal_type*>(options[option_terminal_type::code()].get());
                        for (;;) {
                            char buffer[1024];
                            int bytesReceived = recv(acceptSocket, buffer, sizeof(buffer), 0);
                            if (bytesReceived == SOCKET_ERROR)
                            {
                                std::cerr << "recv failed" << std::endl;
                                closesocket(acceptSocket);
                                return;
                            }
                            auto received = std::string_view{buffer, static_cast<size_t>(bytesReceived)};
                            // are these negotiation bytes?
                            std::string negotiation_reply;
                            while (received.size() >= 3 && received[0] == '\xFF')
                            {
                                // yes, handle
                                size_t skip;
                                negotiation_reply += options.reply(received.data(), skip);
                                received.remove_prefix(skip);
                            }
                            if (!negotiation_reply.empty())
                            {
                                send(acceptSocket, negotiation_reply.c_str(), static_cast<int>(negotiation_reply.size()), 0);
                            }
                            for (char c : received)
                            {
                                if (c == '\b' && !line.empty())
                                {
                                    line.pop_back();
                                    continue;
                                }
                                line += c;
                            }
                            if (terminal_type.terminal_type_ == "ANSI")
                            {
                                echo = std::format("{}{}{}>", 
                                    ansi::clear_line(),
                                    ansi::cursor_position(1, 1),
                                    line);
                                send(acceptSocket, echo.c_str(), static_cast<int>(echo.size()), 0);
                            }
                            else if (options[option_echo::code()]->enabled_) {
                                echo = std::format("{}", line);
                                // send backspaces to erase the line
                                for (size_t i = 0; i < line.size(); ++i)
                                {
                                    echo += '\b';
                                }
                                send(acceptSocket, echo.c_str(), static_cast<int>(echo.size()), 0);
                            }
                            if (!line.empty() && line.back() == '\n')
                            {
                                line.pop_back();
                                if (!line.empty() && line.back() == '\r')
                                {
                                    line.pop_back();
                                }
                                break;
                            }
                        }
                        std::string message = handler(line);
                        send(acceptSocket, message.c_str(), static_cast<int>(message.size()), 0);
                        closesocket(acceptSocket);
                    }).detach();
                    }
                } });
        }
        ~host()
        {
            if (INVALID_SOCKET != socket_)
            {
                closesocket(socket_);
                socket_ = INVALID_SOCKET;
            }
            WSACleanup();
        }

    private:
        SOCKET socket_;
        std::function<bool()> quit_;
        std::jthread thread_;
    };
}