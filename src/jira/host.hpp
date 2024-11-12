#pragma once

#include <format>
#include <iostream>
#include <curl/curl.h>

#include "../conversions/base64.hpp"

namespace jira {
    struct host {
        std::string const base_url = "https://betmavrik.atlassian.net/rest/api/3/";
        host(std::string_view username, std::string_view token) : username_{username}, token_{token} {}

        std::string auth_header() {
            std::string auth = "Authorization: Basic " + 
                conversions::text_to_base64(std::format("{}:{}", username_, token_));
            return auth;
        }

        std::string me() { return get("myself"); }

        // Add the following method to the jira::host struct

        std::string get_assigned_issues() {
            return get_jql("assignee=currentUser()");
        }

        std::string get_jql(std::string_view jql) {
            return get(std::format("search?jql={}", jql));
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

        static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
            ((std::string*)userp)->append((char*)contents, size * nmemb);
            return size * nmemb;
        }
    };
}