#pragma once

#include <string>

namespace database::arangodb
{
    struct login {
        std::string host;
        std::string username;
        std::string password;
        std::string database;
    };
    struct host {
    };
} // namespace database::arangodb
