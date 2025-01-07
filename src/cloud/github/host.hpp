#pragma once

#include <format>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "../../hosting/http/fetch.hpp"
#include "login_host.hpp"

namespace github {
    struct host {
        void login_host(std::shared_ptr<github::login::host> login_host) {
            login_host_ = login_host;
        }

        nlohmann::json const& user() const {
            if (user_.empty()) {
                user_ = fetch_user();
            }
            return user_;
        }

        nlohmann::json organizations() const {
            return nlohmann::json::parse(fetch("https://api.github.com/user/orgs"));
        }

        nlohmann::json fetch_user() const {
            return nlohmann::json::parse(fetch("https://api.github.com/user"));
        }

        nlohmann::json org_repos(std::string_view org) const {
            return nlohmann::json::parse(fetch(std::format("https://api.github.com/orgs/{}/repos", org)));
        }

        nlohmann::json repo_workflows(std::string_view full_name) const {
            return nlohmann::json::parse(fetch(std::format("https://api.github.com/repos/{}/actions/workflows", full_name)));
        }

        http::fetch::header_client_t header_client() const {
            return [bearer_header = std::format("Authorization: Bearer {}", login_host_->personal_token())](auto setheader) {
                setheader(bearer_header);
            };
        }

    private:
        std::string fetch(const std::string &url) const {
            http::fetch fetch;
            return fetch(url, header_client());
        }

        std::shared_ptr<github::login::host> login_host_;
        mutable nlohmann::json user_;
    };
}
