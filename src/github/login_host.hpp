#pragma once

#include <string>

namespace github::login {
    struct host {
        void save_to(auto saver) const {
            saver("token", token_);
        }
        void load_from(auto loader) {
            token_ = *loader("token");
        }
    private:
        std::string token_;
    };
}
