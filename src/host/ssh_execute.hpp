#pragma once

#include <string>

#include <libssh/libssh.h>
#include <libssh/callbacks.h>

struct ssh_execute {
    ssh_execute(std::string_view const &host_name) : host_name_{host_name} {
        ssh_init();
    }

    ~ssh_execute() {
        ssh_finalize();
    }

    std::string execute_command(std::string const &command, unsigned int timeout_seconds = 5) {
        ssh_session session = ssh_new();
        if (session == nullptr) {
            throw std::runtime_error("Error: could not create ssh session");
        }

        ssh_options_set(session, SSH_OPTIONS_HOST, host_name_.c_str());
        ssh_options_set(session, SSH_OPTIONS_PORT_STR, "22");
        // set the timeout
        ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout_seconds);

        int rc = ssh_connect(session);
        if (rc != SSH_OK) {
            ssh_free(session);
            throw std::runtime_error("Error: could not connect to ssh server");
        }

        // Authenticate using public key or password
        rc = ssh_userauth_publickey_auto(session, NULL, NULL);
        if (rc != SSH_AUTH_SUCCESS) {
            ssh_disconnect(session);
            ssh_free(session);
            throw std::runtime_error("Error: could not authenticate to ssh server");
        }

        ssh_channel channel = ssh_channel_new(session);
        if (channel == nullptr) {
            ssh_disconnect(session);
            ssh_free(session);
            throw std::runtime_error("Error: could not create ssh channel");
        }

        rc = ssh_channel_open_session(channel);
        if (rc != SSH_OK) {
            ssh_channel_free(channel);
            ssh_disconnect(session);
            ssh_free(session);
            throw std::runtime_error("Error: could not open ssh channel");
        }

        rc = ssh_channel_request_exec(channel, command.c_str());
        if (rc != SSH_OK) {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            ssh_disconnect(session);
            ssh_free(session);
            throw std::runtime_error("Error: could not execute command");
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
        ssh_disconnect(session);
        ssh_free(session);

        return result;
    }
private:
    std::string host_name_;
};
