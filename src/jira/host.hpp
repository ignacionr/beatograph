#pragma once

#include <format>
#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "../conversions/base64.hpp"
#include "../conversions/uri.hpp"
#include "../http/fetch.hpp"

namespace jira {
    struct host {
        std::string const base_url;
        host(std::string_view username, std::string_view token, std::string_view url)
        : username_{username}, token_{token}, base_url{url} {}

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

        std::string get_assigned_issues(bool include_done = false) {
            std::string jql = "assignee=currentUser()";
            if (!include_done) {
                jql += " AND statusCategory != \"Done\"";
            }
            return get_jql(jql);
        }

        std::string get_browse_url(std::string_view issue_key) {
            return std::format("{}/browse/{}", base_url.substr(0, base_url.find("/rest")), issue_key);
        }

        std::string get_project(int project_id) {
            return get(std::format("project/{}", project_id));
        }

        std::string get_projects() {
            return get("project");
        }

        std::string search_by_project(std::string_view project_key, std::string_view status_category = "To Do") {
            return get_jql(std::format("project={} AND statusCategory=\"{}\" AND issuetype != Subtask", project_key, status_category));
        }
\
        // search by summary
        std::string search_issues_summary(std::string_view partial_jql) {
            return get_jql(std::format("summary~\"{}\"", partial_jql));
        }

        std::string get_jql(std::string_view jql) {
            return get(std::format("search?jql={}", conversions::uri::encode_component(std::string{jql})));
        }

        void transition_issue(std::string_view issue_key, std::string_view transition_id) {
            nlohmann::json::object_t contents = {
                {"transition", {{"id", transition_id}}}
            };
            auto const result_string = post(
                std::format("issue/{}/transitions", issue_key), contents);
            if (!result_string.empty()) {
                auto result = nlohmann::json::parse(result_string);
                throw std::runtime_error(result.at("errorMessages").dump());
            }
        }

        void assign_issue_to_me(std::string_view issue_key) {
            nlohmann::json::object_t contents = {
                {"accountId", account_id()}
            };
            auto const result_string = put(
                std::format("issue/{}/assignee", issue_key), contents);
            if (!result_string.empty()) {
                auto result = nlohmann::json::parse(result_string);
                throw std::runtime_error(result.at("errorMessages").dump());
            }
        }

        void unassign_issue(std::string_view issue_key) {
            nlohmann::json::object_t contents = {
                {"accountId", nullptr}
            };
            auto const result_string = put(
                std::format("issue/{}/assignee", issue_key), contents);
            if (!result_string.empty()) {
                auto result = nlohmann::json::parse(result_string);
                throw std::runtime_error(result.at("errorMessages").dump());
            }
        }

        nlohmann::json get_issue_types() {
            nlohmann::json const result = nlohmann::json::parse(get("issuetype"));
            return result;
        }

        nlohmann::json::object_t create_issue(std::string_view issue_summary, std::string_view project_key, std::string_view issuetype_id) {
            nlohmann::json::object_t contents = {
                {"fields", {
                    {"project", {{"key", project_key}}},
                    {"summary", issue_summary},
                    {"issuetype", {{"id", issuetype_id}}}
                }}
            };
            nlohmann::json::object_t const result = nlohmann::json::parse(post("issue", contents));
            if (result.contains("errorMessages")) {
                throw std::runtime_error(result.at("errorMessages").dump());
            }
            return result;
        }

        nlohmann::json get_issue_comments(std::string_view issue_key) {
            return nlohmann::json::parse(get(std::format("issue/{}/comment", issue_key)));
        }

        auto add_comment(std::string_view issue_key, std::string_view comment_body) {
            nlohmann::json::object_t contents = {
                {"body", {
                    {"type", "doc"},
                    {"version", 1},
                    {"content", {{
                        {"type", "paragraph"},
                        {"content", {{
                            {"type", "text"},
                            {"text", comment_body}
                        }}}
                    }}}
                }}
            };
            auto const result_string = post(
                std::format("issue/{}/comment", issue_key), contents);
            auto const result = nlohmann::json::parse(result_string);
            if (result.contains("errorMessages")) {
                throw std::runtime_error(result_string);
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
        
        std::string post(std::string_view endpoint, nlohmann::json contents) {
            return send(endpoint, "POST", contents.dump(), "application/json");
        }

        std::string get(std::string_view endpoint) {
            http::fetch fetch;
            return fetch(base_url + std::string{endpoint}, [auth_h = auth_header()](auto hs){
                hs("Content-Type: application/json");
                hs(auth_h);
            });
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