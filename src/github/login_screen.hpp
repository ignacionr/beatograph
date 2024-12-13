#pragma once

#include "login_host.hpp"

namespace github::login {
    struct screen {
        screen(host& host): host_{host}
    private:
        host& host_;
    };
}