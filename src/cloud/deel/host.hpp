#pragma once

#include <format>
#include <string>

#include "data.hpp"

namespace cloud::deel {
    template<typename fetch_t>
    struct host {
        std::string token_;

        data::history fetch_history() const noexcept {
            auto json_text = fetch_t{}("https://api.deel.com/invoices/history", 
                [this](auto setter){
                    setter("accept: application/json, text/plain, */*");
                    setter("x-api-version: 2");
                    setter(std::format("x-auth-token: {}", token_));
                });
        }
    };
}