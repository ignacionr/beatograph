#pragma once

#include <format>
#include <string>

#include <curl/curl.h>

namespace dataoffering
{
    struct api_client {
        api_client(std::string_view api_token, std::string_view base_url = "http://57.129.39.115:5000/")
        : api_token_{api_token}, base_url_{base_url} {}

        static size_t read_data(void *ptr, size_t size, size_t nmemb, std::string *data) {
            data->append(static_cast<char *>(ptr), size * nmemb);
            return size * nmemb;
        }

        std::string item_from_url(std::string const &url) {
            CURL *curl = curl_easy_init();
            if (curl) {
                struct curl_slist *headers = nullptr;
                headers = curl_slist_append(headers, std::format("Authorization: Bearer {}", api_token_).c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                std::string response;
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, read_data);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                auto res = curl_easy_perform(curl);
                if (res != CURLE_OK) {
                    std::string const err {curl_easy_strerror(res)};
                    curl_easy_cleanup(curl);
                    throw std::runtime_error(std::format("curl_easy_perform() failed: {}", err));
                }
                curl_easy_cleanup(curl);
                return response;
            }
            throw std::runtime_error("curl_easy_init() failed");
        }

        std::string get_entity(long long id) {
            std::string const url {std::format("{}items/{}", base_url_, id)};
            return item_from_url(url);
        }

        std::string get_entity_update(long long id, std::string const &version) {
            std::string const url {std::format("{}items/{}_{}", base_url_, id, version)};
            return item_from_url(url); 
        }
    private:
        std::string const api_token_;
        std::string const base_url_;
    };
} // namespace dataoffering
