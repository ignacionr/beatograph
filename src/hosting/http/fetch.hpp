#pragma once

#include <format>
#include <functional>
#include <stdexcept>
#include <string>

#include <curl/curl.h>

namespace http {
    struct fetch {
        using header_setter_t = std::function<void(std::string const&)>;
        using header_client_t = std::function<void(header_setter_t)>;
        // typedef for the shape of the fwrite function
        typedef size_t (*write_callback_t)(void *, size_t, size_t, void *);

        std::string operator()(std::string const &url, 
            header_client_t set_headers = [](header_setter_t setheader){},
            write_callback_t write_callback = fetch::write_string,
            void *write_data = nullptr) const
        {
            CURL *curl = curl_easy_init();
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                curl_easy_setopt(curl, CURLOPT_USERAGENT, "beat-o-graph/1.0");
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
                char *pproxy_str = nullptr;
                size_t len = 0;
                if (!_dupenv_s(&pproxy_str, &len, "HTTP_PROXY") && pproxy_str != nullptr) {
                    curl_easy_setopt(curl, CURLOPT_PROXY, pproxy_str);
                }
                struct curl_slist *headers = nullptr;
                auto setheader = [&headers](std::string const& header_value) {
                    headers = curl_slist_append(headers, header_value.c_str());
                };
                set_headers(setheader);
                if (headers) {
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                }
                std::string response;
                if (write_data) {
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_data);
                }
                else if (write_callback == fetch::write_string) {
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                }
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
        static size_t write_string(void *ptr, size_t size, size_t nmemb, void *data) {
            auto *pdata = static_cast<std::string *>(data);
            pdata->append(static_cast<char *>(ptr), size * nmemb);
            return size * nmemb;
        }
    };
}