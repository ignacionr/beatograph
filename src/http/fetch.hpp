#pragma once

#include <format>
#include <stdexcept>
#include <string>

#include <curl/curl.h>

namespace http {
    struct fetch {
        std::string operator()(std::string const &url) const {
            CURL *curl = curl_easy_init();
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string);
                char *pproxy_str = nullptr;
                size_t len = 0;
                if (!_dupenv_s(&pproxy_str, &len, "HTTP_PROXY") && pproxy_str != nullptr) {
                    curl_easy_setopt(curl, CURLOPT_PROXY, pproxy_str);
                }
                std::string response;
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                auto cr = curl_easy_perform(curl);
                if (cr != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    throw std::runtime_error(std::format("Error: {}", curl_easy_strerror(cr)));
                }
                curl_easy_cleanup(curl);
                return response;
            }
            throw std::runtime_error("Error: curl_easy_init failed.");
        }
    private:
        static size_t write_string(void *ptr, size_t size, size_t nmemb, std::string *data) {
            data->append(static_cast<char *>(ptr), size * nmemb);
            return size * nmemb;
        }
    };
}