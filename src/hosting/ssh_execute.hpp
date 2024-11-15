#pragma once

#include <format>
#include <mutex>
#include <string>

#include <libssh/libssh.h>
#include <libssh/callbacks.h>

struct ssh_execute {
    ssh_execute(std::string_view const &host_name, unsigned int timeout_seconds = 5) : host_name_{host_name} {
        ssh_init();
        session = ssh_new();
        if (session == nullptr) {
            throw std::runtime_error("Error: could not create ssh session");
        }

        ssh_options_set(session, SSH_OPTIONS_HOST, host_name_.c_str());
        ssh_options_set(session, SSH_OPTIONS_PORT_STR, "22");
        // set the timeout
        ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout_seconds);

        int rc = ssh_connect(session);
        if (rc != SSH_OK) {
            throw std::runtime_error("Error: could not connect to ssh server");
        }
        connected = true;

        // Authenticate using public key or password
        rc = ssh_userauth_publickey_auto(session, NULL, NULL);
        if (rc != SSH_AUTH_SUCCESS) {
            throw std::runtime_error("Error: could not authenticate to ssh server");
        }
    }

    ~ssh_execute() {
        if (session != nullptr) {
            if (connected) {
                ssh_disconnect(session);
            }
            ssh_free(session);
        }
        ssh_finalize();
    }

    std::string execute_command(std::string const &command) 
    {
        ssh_channel channel;
        std::lock_guard<std::mutex> lock(mutex);
        channel = ssh_channel_new(session);
        if (channel == nullptr) {
            throw std::runtime_error("Error: could not create ssh channel");
        }

        auto rc = ssh_channel_open_session(channel);
        if (rc != SSH_OK) {
            // obtain the error description
            const char *error_description = ssh_get_error(session);
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            throw std::runtime_error(std::format("Error: could not open ssh channel: {}", error_description));
        }

        rc = ssh_channel_request_exec(channel, command.c_str());
        if (rc != SSH_OK) {
            const char *error_description = ssh_get_error(session);
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            throw std::runtime_error(std::format("Error: could not execute command: {}", error_description));
        }

        std::string result;
        char buffer[256];
        int nbytes;
        while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0)) > 0) {
            result.append(buffer, nbytes);
        }

        ssh_channel_send_eof(channel);
        ssh_channel_close(channel);
        ssh_channel_free(channel);

        return result;
    }

private:
    std::string host_name_;
    ssh_session session{nullptr};
    bool connected{false};
    std::mutex mutex;
};
