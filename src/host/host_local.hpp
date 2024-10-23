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

struct host_local
{
    struct running_process
    {
        using sink_t = std::function<void(std::string_view)>;

        running_process(const char *command)
        {
            ZeroMemory(&pi, sizeof(pi));
            pi.hProcess = INVALID_HANDLE_VALUE;
            pi.hThread = INVALID_HANDLE_VALUE;
            // Create a pipe for the child process to write to
            SECURITY_ATTRIBUTES saAttr;
            saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
            saAttr.lpSecurityDescriptor = NULL;
            saAttr.bInheritHandle = TRUE;

            if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0))
            {
                throw std::runtime_error("CreatePipe failed!");
            }

            // Ensure the read handle is not inherited
            if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0))
            {
                throw std::runtime_error("SetHandleInformation failed!");
            }

            // Set up the STARTUPINFO structure
            STARTUPINFO si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = hWritePipe;
            si.hStdError = hWritePipe;

            // Construct the command line
            std::string command_str = "cmd /c ";
            command_str += command;

            // Create a mutable buffer for lpCommandLine
            std::vector<char> command_line_vec(command_str.begin(), command_str.end());
            command_line_vec.push_back('\0'); // Ensure null-termination

            // Create the child process
            if (!CreateProcessA(NULL, command_line_vec.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
            {
                // Obtain the error code
                DWORD error = GetLastError();
                // Obtain the error description
                char error_description[256];
                FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, error_description, 256, NULL);
                throw std::runtime_error(std::format("CreateProcess failed! Error code: {} Error description: {}", error, error_description));
            }

            // Close the write end of the pipe in the parent process
            CloseHandle(hWritePipe), hWritePipe = INVALID_HANDLE_VALUE;
        }
        ~running_process()
        {
            if (hReadPipe != INVALID_HANDLE_VALUE)
            {
                CloseHandle(hReadPipe);
            }
            if (hWritePipe != INVALID_HANDLE_VALUE)
            {
                CloseHandle(hWritePipe);
            }
            if (pi.hProcess != INVALID_HANDLE_VALUE)
            {
                CloseHandle(pi.hProcess);
            }
            if (pi.hThread != INVALID_HANDLE_VALUE)
            {
                CloseHandle(pi.hThread);
            }
        }

        DWORD wait(DWORD milliseconds = INFINITE)
        {
            if (pi.hProcess != INVALID_HANDLE_VALUE) {
                WaitForSingleObject(pi.hProcess, milliseconds);
                GetExitCodeProcess(pi.hProcess, &exitCode);
                CloseHandle(pi.hProcess), pi.hProcess = INVALID_HANDLE_VALUE;
            }
            return exitCode;
        }

        void stop() {
            if (pi.hProcess != INVALID_HANDLE_VALUE) {
                TerminateProcess(pi.hProcess, 0);
            }
        }

        void sendQuitSignal() 
        {
            if (pi.hProcess != INVALID_HANDLE_VALUE) {
                // Check if process is a console app, send CTRL_BREAK_EVENT
                if (AttachConsole(pi.dwProcessId)) {
                    // Send CTRL_BREAK_EVENT to console processes
                    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pi.dwProcessId);
                } else {
                    // For GUI apps, try sending WM_CLOSE
                    HWND hWnd = FindWindow(nullptr, nullptr); // Adjust the window finding as needed
                    if (hWnd) {
                        PostMessage(hWnd, WM_CLOSE, 0, 0);
                    }
                }
            }
        }

        void read_all(sink_t sink) {
            // Read from the pipe
            char buffer[128];
            DWORD bytesRead;
            while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead != 0)
            {
                sink(std::string_view{buffer, bytesRead});
            }
            // Close the read end of the pipe
            CloseHandle(hReadPipe), hReadPipe = INVALID_HANDLE_VALUE;
        }

    private:
        PROCESS_INFORMATION pi;
        HANDLE hReadPipe{INVALID_HANDLE_VALUE}, hWritePipe{INVALID_HANDLE_VALUE};
        DWORD exitCode{};
    };

    auto run(const char *command) {
        return std::make_unique<running_process>(command);
    }

    void execute_command(const char *command, auto sink)
    {
        auto proc{run(command)};
        proc->read_all(sink);
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
        bool isInUse = false;

        WSADATA wsaData;
        SOCKET ListenSocket = INVALID_SOCKET;

        struct addrinfo *result = NULL, hints;

        // Initialize Winsock
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0)
        {
            return false;
        }

        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;       // IPv4
        hints.ai_socktype = SOCK_STREAM; // TCP
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE; // Listen on all interfaces

        char portStr[6];
        sprintf_s(portStr, "%u", port);

        // Resolve the local address and port to be used by the server
        iResult = getaddrinfo(NULL, portStr, &hints, &result);
        if (iResult != 0)
        {
            WSACleanup();
            return false;
        }

        // Create a SOCKET for connecting to the server
        ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (ListenSocket == INVALID_SOCKET)
        {
            freeaddrinfo(result);
            WSACleanup();
            return false;
        }

        // Attempt to bind to the port
        iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
        if (iResult == SOCKET_ERROR)
        {
            // If bind failed, port is likely in use
            isInUse = true;
        }

        // Cleanup
        closesocket(ListenSocket);
        freeaddrinfo(result);
        WSACleanup();

        return isInUse;
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
