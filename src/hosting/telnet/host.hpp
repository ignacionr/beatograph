#pragma once

#include "pch.h"

#include <functional>
#include <stdexcept>
#include <thread>

namespace hosting::telnet
{
    class host
    {
    public:
        using handler_t = std::function<std::string(std::string_view)>;
        host(std::function<bool()> quit, handler_t handler) {
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
            if (bind(socket_, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
            {
                closesocket(socket_);
                throw std::runtime_error("bind failed");
            }
            thread_ = std::jthread([this, quit, handler]() {
                listen(socket_, 5);
                while (!quit())
                {
                    SOCKET acceptSocket = accept(socket_, NULL, NULL);
                    if (acceptSocket == INVALID_SOCKET)
                    {
                        if (INVALID_SOCKET != socket_)
                            closesocket(socket_);
                    }
                    std::thread([acceptSocket, handler]() {
                        std::string line;
                        for (;;) {
                            char buffer[1024];
                            int bytesReceived = recv(acceptSocket, buffer, sizeof(buffer), 0);
                            if (bytesReceived == SOCKET_ERROR)
                            {
                                std::cerr << "recv failed" << std::endl;
                                closesocket(acceptSocket);
                                return;
                            }
                            line += std::string(buffer, bytesReceived);
                            if (line.back() == '\n')
                            {
                                line.pop_back();
                                if (line.back() == '\r')
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
            });
        }
        ~host(){
            if (INVALID_SOCKET != socket_) {
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