#pragma once

#include <format>
#include <string>
#include <stdexcept>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "../conversions/base64.hpp"

class toggl_client
{
public:
    toggl_client(const std::string &apiToken) : baseUrl("https://api.track.toggl.com/api/v9/")
    {
        std::string base_auth = apiToken + ":api_token";
        std::string base64_auth_string = conversions::text_to_base64(base_auth);
        auth_header = std::format("Authorization: Basic {}", base64_auth_string);
    }

    auto getTimeEntries()
    {
        auto str = performRequest(std::format("{}me/time_entries", baseUrl), "GET");
        return nlohmann::json::parse(str);
    }

    std::string startTimeEntry(long long workspace_id, const std::string_view description_text)
    {
        auto const json {std::format(R"({{
            "created_with": "Beat-o-graph",
            "description":"{}",
            "duration": -1,
            "start": "{:%FT%T}Z",
            "tags": ["adhoc"],
            "workspace_id": {}
        }})", 
        description_text,
        std::chrono::system_clock::now(),
        workspace_id
        )};
        return performRequest(
            std::format("{}workspaces/{}/time_entries", baseUrl, workspace_id), 
            "POST", 
            json);
    }

    std::string stopTimeEntry(auto &entry)
    {
        auto url = std::format("{}workspaces/{}/time_entries/{}", 
            baseUrl,
            entry["workspace_id"].get<long long>(),
            entry["id"].get<long long>());
        auto data = std::format("{{\"stop\":\"{}\"}}", 
            std::format("{:%FT%T}Z", std::chrono::system_clock::now())
        );
        return performRequest(url, "PUT", data);
    }

    void deleteTimeEntry(auto &entry)
    {
        auto url = std::format("{}workspaces/{}/time_entries/{}", 
            baseUrl,
            entry["workspace_id"].get<long long>(),
            entry["id"].get<long long>());
        performRequest(url, "DELETE");
    }

private:
    std::string auth_header;
    std::string baseUrl;

    std::string performRequest(const std::string &url, const std::string &method, const std::string &data = "")
    {
        CURL *curl = curl_easy_init();
        if (curl)
        {
            struct curl_slist *headers = nullptr;
            headers = curl_slist_append(headers, auth_header.c_str());
            headers = curl_slist_append(headers, "Accept: application/json");
            if (!data.empty())
            {
                headers = curl_slist_append(headers, "Content-Type: application/json");
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            if (method != "GET")
            {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
            }
            if (!data.empty())
            {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
            }
            else if (method != "GET")
            {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
            }
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            std::string response;
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            try
            {
                CURLcode res = curl_easy_perform(curl);
                if (res != CURLE_OK)
                {
                    throw std::runtime_error("Failed to perform request: " + std::string(curl_easy_strerror(res)));
                }
                // obtain the response code
                long response_code;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
                if (response_code != 200)
                {
                    throw std::runtime_error(std::format("Request failed with code: {} and message {}", response_code, response));
                }
            }
            catch(...) {
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                throw;
            }
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return response;
        }
        throw std::runtime_error("Failed to initialize cURL");
    }
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        ((std::string *)userp)->append((char *)contents, size * nmemb);
        return size * nmemb;
    }
};
