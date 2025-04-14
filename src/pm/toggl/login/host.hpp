#pragma once

#include <format>
#include <string>
#include "../../../util/conversions/base64.hpp"

namespace toggl::login
{
    struct host {
        void save_to(auto saver) const {
            saver("auth_header", auth_header);
        }
        void load_from(auto loader) {
            auth_header = *loader("auth_header");
        }
        void set_token(std::string_view token) {
            std::string base_auth = std::format("{}:api_token", token);
            std::string base64_auth_string = conversions::text_to_base64(base_auth);
            auth_header = std::format("Authorization: Basic {}", base64_auth_string);
        }
        std::string const &header() const {
            return auth_header;
        }
    private:
        std::string auth_header;
    };
} // namespace toggl::login
