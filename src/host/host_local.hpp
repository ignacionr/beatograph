#pragma once

#include <iostream>
#include <cstdio> // for popen and pclose
#include <stdio.h>
#include <memory> // for smart pointers
#include <string> // for strings
#include <stdexcept>
#include <format>

struct host_local
{
    std::string execute_command(const char *command)
    {
        std::string result;

#ifdef _WIN32
        // Create a pipe for the child process to write to
        HANDLE hReadPipe, hWritePipe;
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
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
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
        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));
        if (!CreateProcess(NULL, command_line_vec.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        {
            // Obtain the error code
            DWORD error = GetLastError();
            // Obtain the error description
            char error_description[256];
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, error_description, 256, NULL);

            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            throw std::runtime_error(std::format("CreateProcess failed! Error code: {} Error description: {}", error, error_description));
        }

        // Close the write end of the pipe in the parent process
        CloseHandle(hWritePipe);

        // Read from the pipe
        char buffer[128];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead != 0)
        {
            result.append(buffer, bytesRead);
        }

        // Close the read end of the pipe
        CloseHandle(hReadPipe);

        // Wait for the child process to finish
        WaitForSingleObject(pi.hProcess, INFINITE);

        // Obtain the exit code
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        if (exitCode != 0)
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            throw std::runtime_error("Child process failed!");
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
#else
        // Your existing code for non-Windows platforms
        std::array<char, 128> buffer;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
        if (!pipe)
        {
            throw std::runtime_error("popen() failed!");
        }

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
        {
            result += buffer.data();
        }
#endif

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
