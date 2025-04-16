#pragma once

#include <format>
#include <functional>

#include "ssh_execute.hpp"
#if defined(_WIN32) || defined(_WIN64)
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <signal.h>
#endif


namespace hosting::local
{
    struct running_process
    {
        using sink_t = std::function<void(std::string_view)>;
        using environment_getter_t = std::function<std::string(std::string_view)>;

        running_process(std::string_view command, environment_getter_t env, bool include_stderr = true)
        {
#if defined(_WIN32) || defined(_WIN64)
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
            if (include_stderr) si.hStdError = hWritePipe;

            // Construct the command line
            std::string command_str = std::string{command.data(), command.size()};
            command_str = env(command_str);

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
#else
            // Posix
            // Create a pipe for the child process to write to
            if (pipe(pipefd_) == -1)
            {
                throw std::runtime_error("Pipe failed!");
            }
            // Fork the process
            pid_ = fork();
            if (pid_ == -1)
            {
                throw std::runtime_error("Fork failed!");
            }
            else if (pid_ == 0)
            {
                // Child process
                dup2(pipefd_[1], STDOUT_FILENO);
                dup2(pipefd_[1], STDERR_FILENO);
                close(pipefd_[0]);
                close(pipefd_[1]);
                execlp(command.data(), command.data(), NULL);
                _exit(EXIT_FAILURE); // If execlp fails
            }
            else
            {
                // Parent process
                close(pipefd_[1]);
            }
#endif
        }

        ~running_process()
        {
            close_pipes();
#if defined(_WIN32) || defined(_WIN64)
            if (pi.hProcess != INVALID_HANDLE_VALUE)
            {
                CloseHandle(pi.hProcess), pi.hProcess = INVALID_HANDLE_VALUE;
            }
            if (pi.hThread != INVALID_HANDLE_VALUE)
            {
                CloseHandle(pi.hThread), pi.hThread = INVALID_HANDLE_VALUE;
            }
#else
            if (pid_ > 0)
            {
                close(pipefd_[0]);
                kill(pid_, SIGTERM);
                waitpid(pid_, &exitCode_, 0);
            }
#endif
        }

        void close_pipes()
        {
#if defined(_WIN32) || defined(_WIN64)
            if (hReadPipe != INVALID_HANDLE_VALUE)
            {
                try
                {
                    read_all([](std::string_view) {});
                }
                catch (...)
                {
                    // ignore since we might be just inside a destructor
                }
                CloseHandle(hReadPipe), hReadPipe = INVALID_HANDLE_VALUE;
            }
            if (hWritePipe != INVALID_HANDLE_VALUE)
            {
                CloseHandle(hWritePipe), hWritePipe = INVALID_HANDLE_VALUE;
            }
#else
            if (pipefd_[0] != -1)
            {
                close(pipefd_[0]), pipefd_[0] = -1;
            }
            if (pipefd_[1] != -1)
            {
                close(pipefd_[1]), pipefd_[1] = -1;
            }
#endif
        }

        auto wait(unsigned int milliseconds = 3600000)
        {
#if defined(_WIN32) || defined(_WIN64)
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
#else
            if (pid_ > 0)
            {
                auto const res = waitpid(pid_, &exitCode_, WNOHANG);
                if (res == -1)
                {
                    throw std::runtime_error("waitpid failed!");
                }
            }
            return exitCode_;
#endif
        }

        void stop()
        {
#if defined(_WIN32) || defined(_WIN64)
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
#else
            if (pid_ > 0)
            {
                close_pipes();
                kill(pid_, SIGKILL);
            }
#endif
        }

        void sendQuitSignal(
#if defined(_WIN32) || defined(_WIN64) 
            unsigned int code = CTRL_BREAK_EVENT
#endif
        )
        {
#if defined(_WIN32) || defined(_WIN64)
            if (pi.hProcess != INVALID_HANDLE_VALUE)
            {
                GenerateConsoleCtrlEvent(code, pi.dwProcessId);
            }
#else
            if (pid_ > 0)
            {
                kill(pid_, SIGINT);
            }
#endif
        }

        void read_all(sink_t sink, unsigned int timeout = 5000)
        {
#if defined(_WIN32) || defined(_WIN64)
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
                    if (bytesRead > 0)
                    {
                        if (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead != 0)
                        {
                            auto contents{std::string_view{buffer, bytesRead}};
                            sink(contents);
                        }
                        else
                        {
                            break; // No more data to read
                        }
                    }
                    else
                    {
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
#else
            // Posix
            char buffer[128];
            ssize_t bytesRead;
            for (;;)
            {
                bytesRead = read(pipefd_[0], buffer, sizeof(buffer));
                if (bytesRead == -1)
                {
                    throw std::runtime_error("Read failed!");
                }
                else if (bytesRead == 0)
                {
                    break; // No more data to read
                }
                else
                {
                    auto contents{std::string_view{buffer, static_cast<size_t>(bytesRead)}};
                    sink(contents);
                }
            }
            // Close the read end of the pipe
            close(pipefd_[0]), pipefd_[0] = -1;
#endif
        }

    private:
#if defined(_WIN32) || defined(_WIN64)
        PROCESS_INFORMATION pi;
        HANDLE hReadPipe{INVALID_HANDLE_VALUE}, hWritePipe{INVALID_HANDLE_VALUE};
        DWORD exitCode{};
#else
        pid_t pid_{};
        int pipefd_[2]{};
        int exitCode_{};
#endif
    };
}