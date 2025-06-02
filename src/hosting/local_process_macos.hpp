#pragma once

#include <format>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/select.h>
#include <cstring>

namespace hosting::local
{
    struct running_process
    {
        using sink_t = std::function<void(std::string_view)>;
        using environment_getter_t = std::function<std::string(std::string_view)>;

        running_process(std::string_view command, environment_getter_t env, bool include_stderr = true)
        {
            // Create pipe for reading output
            if (pipe(pipe_fd) == -1) {
                throw std::runtime_error("Failed to create pipe");
            }

            // Set read end to non-blocking
            int flags = fcntl(pipe_fd[0], F_GETFL, 0);
            fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK);

            pid_ = fork();
            if (pid_ == -1) {
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                throw std::runtime_error("Failed to fork process");
            }

            if (pid_ == 0) {
                // Child process
                close(pipe_fd[0]); // Close read end
                
                // Redirect stdout to pipe
                if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
                    exit(1);
                }
                
                // Redirect stderr to pipe if requested
                if (include_stderr && dup2(pipe_fd[1], STDERR_FILENO) == -1) {
                    exit(1);
                }
                
                close(pipe_fd[1]);

                // Execute command through shell
                std::string resolved_command = env(std::string{command});
                execl("/bin/sh", "sh", "-c", resolved_command.c_str(), nullptr);
                exit(1);
            } else {
                // Parent process
                close(pipe_fd[1]); // Close write end
            }
        }

        ~running_process()
        {
            if (pipe_fd[0] != -1) close(pipe_fd[0]);
            if (pid_ > 0) {
                kill(pid_, SIGTERM);
                waitpid(pid_, nullptr, 0);
            }
        }

        void close_pipes()
        {
            if (pipe_fd[0] != -1) {
                close(pipe_fd[0]);
                pipe_fd[0] = -1;
            }
        }

        int wait(int milliseconds = -1)
        {
            int status;
            if (milliseconds == -1) {
                waitpid(pid_, &status, 0);
            } else {
                // Use non-blocking wait with timeout
                struct timespec timeout = {milliseconds / 1000, (milliseconds % 1000) * 1000000};
                if (waitpid(pid_, &status, WNOHANG) == 0) {
                    nanosleep(&timeout, nullptr);
                    waitpid(pid_, &status, WNOHANG);
                }
            }
            return WEXITSTATUS(status);
        }

        void stop()
        {
            if (pid_ > 0) {
                kill(pid_, SIGTERM);
            }
        }

        void sendQuitSignal(int signal = SIGINT)
        {
            if (pid_ > 0) {
                kill(pid_, signal);
            }
        }

        void read_all(sink_t sink, int timeout_ms = 5000)
        {
            char buffer[4096];
            fd_set readfds;
            struct timeval timeout = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
            
            while (true) {
                FD_ZERO(&readfds);
                FD_SET(pipe_fd[0], &readfds);
                
                int result = select(pipe_fd[0] + 1, &readfds, nullptr, nullptr, &timeout);
                if (result <= 0) break; // Timeout or error
                
                if (FD_ISSET(pipe_fd[0], &readfds)) {
                    ssize_t bytes_read = read(pipe_fd[0], buffer, sizeof(buffer));
                    if (bytes_read > 0) {
                        sink(std::string_view{buffer, static_cast<size_t>(bytes_read)});
                    } else {
                        break; // EOF or error
                    }
                }
            }
        }

    private:
        pid_t pid_ = -1;
        int pipe_fd[2] = {-1, -1};
    };
}
