#pragma once

#include <format>
#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "../conversions/base64.hpp"
#include "../conversions/uri.hpp"

namespace jira {
    struct host {
        std::string const base_url = "https://betmavrik.atlassian.net/rest/api/3/";
        host(std::string_view username, std::string_view token) : username_{username}, token_{token} {}

        std::string auth_header() {
            std::string auth = "Authorization: Basic " + 
                conversions::text_to_base64(std::format("{}:{}", username_, token_));
            return auth;
        }

        nlohmann::json::object_t me() 
        {
            auto response = get("myself");
            auto json = nlohmann::json::parse(response);
            account_id_ = json.at("accountId").get<std::string>();
            return json;
        }

        std::string account_id() {
            if (!account_id_.has_value()) {
                me();
            }
            return account_id_.value();
        }

        // Add the following method to the jira::host struct

        std::string get_assigned_issues(bool include_done = false) {
            std::string jql = "assignee=currentUser()";
            if (!include_done) {
                jql += " AND statusCategory != \"Done\"";
            }
            return get_jql(jql);
        }

        std::string get_project(int project_id) {
            return get(std::format("project/{}", project_id));
        }

        std::string search_todo(std::string project_key) {
            return get_jql(std::format("project={} AND statusCategory=\"To Do\"", project_key));
        }
\
        // search by summary
        std::string search_issues_summary(std::string_view partial_jql) {
            return get_jql(std::format("summary~\"{}\"", partial_jql));
        }

        std::string get_jql(std::string_view jql) {
            return get(std::format("search?jql={}", conversions::uri::encode_component(std::string{jql})));
        }

        nlohmann::json assign_issue_to_me(std::string_view issue_key) {
            nlohmann::json::object_t contents = {
                {"accountId", account_id()}
            };
            auto const result_string = put(
                std::format("issue/{}/assignee", issue_key), contents);
            auto result = nlohmann::json::parse(result_string);
            if (result.contains("errorMessages")) {
                throw std::runtime_error(result.at("errorMessages").dump());
            }
            return result;
        }

        std::string send(std::string_view endpoint, std::string_view verb, std::string_view contents, std::string_view content_type){
            CURL* curl = curl_easy_init();
            if(!curl) {
                return "CURL initialization failed";
            }

            std::string url = base_url + std::string{endpoint};
            std::string response;
            struct curl_slist* headers = nullptr;

            auto const auth {auth_header()};
            headers = curl_slist_append(headers, auth.c_str());
            headers = curl_slist_append(headers, std::format("Content-Type: {}", content_type).c_str());

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, verb.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, contents.data());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                curl_easy_cleanup(curl);
                return "CURL request failed";
            }

            curl_easy_cleanup(curl);
            return response;
        }

        std::string put(std::string_view endpoint, nlohmann::json contents) {
            return send(endpoint, "PUT", contents.dump(), "application/json");
        }

        std::string get(std::string_view endpoint) {
            CURL* curl = curl_easy_init();
            if(!curl) {
                return "CURL initialization failed";
            }

            std::string url = base_url + std::string{endpoint};
            std::string response;
            struct curl_slist* headers = nullptr;

            auto const auth {auth_header()};
            headers = curl_slist_append(headers, auth.c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            // Enable verbose mode
            // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

            // disable proxy
            // curl_easy_setopt(curl, CURLOPT_NOPROXY, "betmavrik.atlassian.net");
            
            // Disable SSL verification (for testing only)
            // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                curl_easy_cleanup(curl);
                return "CURL request failed";
            }

            curl_easy_cleanup(curl);
            return response;
        }

    private:
        std::string username_;
        std::string token_;
        std::optional<std::string> account_id_;

        static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
            ((std::string*)userp)->append((char*)contents, size * nmemb);
            return size * nmemb;
        }
    };
}