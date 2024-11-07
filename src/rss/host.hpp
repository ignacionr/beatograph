#pragma once

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <thread>

#include <curl/curl.h>
#include "feed.hpp"

namespace rss {
    struct host {

        static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp) {
            auto parser = static_cast<rss::feed*>(userp);
            (*parser)(std::string_view{static_cast<char*>(contents), size * nmemb});
            return size * nmemb;
        }

        static feed get_feed(std::string_view url) 
        {
            // use curl to obtain the feed contents
            CURL *curl = curl_easy_init();
            if (!curl) {
                throw std::runtime_error("Failed to initialize curl");
            }
            struct curl_slist *headers = nullptr;
            headers = curl_slist_append(headers, "Accept: application/rss+xml");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_URL, url.data());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            feed parser;
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &parser);
            CURLcode res;
            try {
                res = curl_easy_perform(curl);
            }
            catch (...) {
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                throw;
            }
            curl_slist_free_all(headers);
            if (res != CURLE_OK) {
                curl_easy_cleanup(curl);
                throw std::runtime_error("Failed to perform request: " + std::string(curl_easy_strerror(res)));
            }
            // check the status code
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            curl_easy_cleanup(curl);
            if (response_code >= 400) {
                throw std::runtime_error("Failed to fetch feed: " + std::to_string(response_code));
            }
            return parser;
        }

        void add_feed(std::string_view url) {
            std::string url_str{url};
            std::thread([this, url_str] {
                add_feed_sync(url_str);
            }).detach();
        }

        void add_feed_sync(std::string_view url) {
            auto feed = get_feed(url);
            auto feeds = feeds_.load();
            if (feeds == nullptr) {
                feeds = std::make_shared<std::vector<std::shared_ptr<rss::feed>>>();
            }
            auto feed_ptr = std::make_shared<rss::feed>(std::move(feed));
            feeds->emplace_back(std::move(feed_ptr));
            feeds_.store(feeds);
        }
    private:
        std::atomic<std::shared_ptr<std::vector<std::shared_ptr<rss::feed>>>> feeds_;
    };
}