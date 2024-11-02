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
            std::cerr << std::format("Process@{} running {}\n", reinterpret_cast<void*>(this), command);
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
            STARTUPINFOA si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = hWritePipe;
            si.hStdError = hWritePipe;

            // Construct the command line
            std::string command_str = command;

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
            close_pipes();
            if (pi.hProcess != INVALID_HANDLE_VALUE)
            {
                CloseHandle(pi.hProcess), pi.hProcess = INVALID_HANDLE_VALUE;
            }
            if (pi.hThread != INVALID_HANDLE_VALUE)
            {
                CloseHandle(pi.hThread), pi.hThread = INVALID_HANDLE_VALUE;
            }
        }

        void close_pipes()
        {
            if (hReadPipe != INVALID_HANDLE_VALUE)
            {
                try {
                    read_all([](std::string_view){});
                }
                catch(...) {
                    // ignore since we might be just inside a destructor
                }
                CloseHandle(hReadPipe), hReadPipe = INVALID_HANDLE_VALUE;
            }
            if (hWritePipe != INVALID_HANDLE_VALUE)
            {
                CloseHandle(hWritePipe), hWritePipe = INVALID_HANDLE_VALUE;
            }
        }

        DWORD wait(DWORD milliseconds = INFINITE)
        {
            if (pi.hProcess != INVALID_HANDLE_VALUE)
            {
                auto const res = WaitForSingleObject(pi.hProcess, milliseconds);
                if (res == WAIT_TIMEOUT)
                {
                    return res;
                }
                GetExitCodeProcess(pi.hProcess, &exitCode);
                CloseHandle(pi.hProcess), pi.hProcess = INVALID_HANDLE_VALUE;
            }
            return exitCode;
        }

        void stop()
        {
            if (pi.hProcess != INVALID_HANDLE_VALUE)
            {
                close_pipes();
                if (!TerminateProcess(pi.hProcess, 0))
                {
                    // get the error code
                    DWORD error = GetLastError();
                    // get the error description
                    char error_description[256];
                    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, error_description, 256, NULL);
                    throw std::runtime_error(std::format("TerminateProcess failed! Error code: {} Error description: {}", error, error_description));
                }
            }
        }

        void sendQuitSignal(DWORD code = CTRL_BREAK_EVENT)
        {
            if (pi.hProcess != INVALID_HANDLE_VALUE)
            {
                GenerateConsoleCtrlEvent(code, pi.dwProcessId);
            }
        }

        void read_all(sink_t sink, DWORD timeout = 5000)
        {
            // Read from the pipe
            char buffer[128];
            DWORD bytesRead;
            for (;;)
            {
                // Wait for data to be available on the pipe with a 5 second timeout
                DWORD waitResult = WaitForSingleObject(hReadPipe, timeout);
                if (waitResult == WAIT_TIMEOUT)
                {
                    throw std::runtime_error("Read operation timed out!");
                }
                else if (waitResult == WAIT_OBJECT_0)
                {
                    // check that the pipe has data
                    if (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &bytesRead, NULL) == 0)
                    {
                        // obtain the error code
                        DWORD error = GetLastError();
                        if (error == ERROR_BROKEN_PIPE)
                        {
                            break; // Pipe is broken, no more data to read
                        }
                        // obtain the error description
                        char error_description[256];
                        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, error_description, 256, NULL);
                        throw std::runtime_error(std::format("PeekNamedPipe failed! Error code: {} Error description: {}", error, error_description));
                    }
                    if (bytesRead > 0) {
                        if (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead != 0)
                        {
                            auto contents {std::string_view{buffer, bytesRead}};
                            std::cerr << std::format("Proces@{} received << \n{}\n >>\n({} bytes)\n", reinterpret_cast<void*>(this), std::string{contents}, bytesRead);
                            sink(contents);
                        }
                        else
                        {
                            break; // No more data to read
                        }
                    }
                    else {
                        // this is odd, the handle was signaled but there's nothing to read; for the time being, do nothing
                    }
                }
                else
                {
                    throw std::runtime_error("WaitForSingleObject failed!");
                }
            }
            // Close the read end of the pipe
            CloseHandle(hReadPipe), hReadPipe = INVALID_HANDLE_VALUE;
        }

    private:
        PROCESS_INFORMATION pi;
        HANDLE hReadPipe{INVALID_HANDLE_VALUE}, hWritePipe{INVALID_HANDLE_VALUE};
        DWORD exitCode{};
    };

    auto run(const char *command)
    {
        return std::make_unique<running_process>(command);
    }

    void execute_command(const char *command, auto sink)
    {
        auto proc{run(command)};
        proc->read_all(sink);
    }

    std::string ssh(std::string_view command, std::string_view host_name, unsigned int timeout_seconds = 5) {
        auto cmd = std::format("ssh -o ConnectTimeout={} {} {}", timeout_seconds, host_name, command);
        return execute_command(cmd.c_str());
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
};
