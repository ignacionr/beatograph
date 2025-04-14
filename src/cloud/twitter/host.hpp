#pragma once

#include <string>

namespace cloud::twitter {
    struct host {
        std::string name;
        std::string url;
        std::string api_url;
        std::string api_key;
        std::string api_secret_key;
        std::string bearer_token;
        std::string access_token;
        std::string access_token_secret;

        
    };

} // namespace cloud::twitter
