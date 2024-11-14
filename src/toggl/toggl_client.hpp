#pragma once

#include <format>
#include <string>
#include <stdexcept>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

class toggl_client
{
public:
    toggl_client(const std::string &apiToken) : baseUrl("https://api.track.toggl.com/api/v9/")
    {
        std::string base_auth = apiToken + ":api_token";
        std::string base64_auth_string = base64_encode(base_auth);
        auth_header = "Authorization: Basic " + base64_auth_string;
    }

    auto getTimeEntries()
    {
        auto str = performRequest(baseUrl + "me/time_entries", "GET");
        return nlohmann::json::parse(str);
    }
    std::string startTimeEntry(const std::string &description)
    {
        return performRequest(baseUrl + "time_entries/start", "POST", "{\"time_entry\":{\"description\":\"" + description + "\"}}");
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

private:
    std::string auth_header;
    std::string baseUrl;

    static std::string base64_encode(std::string const &src)
    {
        static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string ret;
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];
        for (const auto &c : src)
        {
            char_array_3[i++] = c;
            if (i == 3)
            {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;
                for (i = 0; (i < 4); i++)
                {
                    ret += base64_chars[char_array_4[i]];
                }
                i = 0;
            }
        }
        if (i)
        {
            for (j = i; j < 3; j++)
            {
                char_array_3[j] = '\0';
            }
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (j = 0; (j < i + 1); j++)
            {
                ret += base64_chars[char_array_4[j]];
            }
            while ((i++ < 3))
            {
                ret += '=';
            }
        }
        return ret;
    }

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
