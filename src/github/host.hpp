#pragma once

#include <memory>
#include "login_host.hpp"

namespace github {
    struct host {
        void login_host(github::login::host login_host) {
            login_host_ = login_host;
        }
    private:
        github::login::host login_host_;
    };
}
