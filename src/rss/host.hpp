#pragma once

#include <algorithm>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <thread>

#include <curl/curl.h>
#include "feed.hpp"

namespace rss
{
    struct host
    {
        host(std::function<std::string(std::string_view)> system_runner) : system_runner_(system_runner)
        {
        }

        static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
        {
            auto parser = static_cast<rss::feed *>(userp);
            (*parser)(std::string_view{static_cast<char *>(contents), size * nmemb});
            return size * nmemb;
        }

        static feed get_feed(std::string_view url, std::function<std::string(std::string_view)> system_runner)
        {
            // use curl to obtain the feed contents
            CURL *curl = curl_easy_init();
            if (!curl)
            {
                throw std::runtime_error("Failed to initialize curl");
            }
            struct curl_slist *headers = nullptr;
            headers = curl_slist_append(headers, "Accept: */*");
            headers = curl_slist_append(headers, "User-Agent: beatograph/1.0");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_URL, url.data());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            feed parser{system_runner};
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &parser);
            CURLcode res;
            try
            {
                res = curl_easy_perform(curl);
            }
            catch (...)
            {
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                throw;
            }
            curl_slist_free_all(headers);
            if (res != CURLE_OK)
            {
                curl_easy_cleanup(curl);
                throw std::runtime_error("Failed to perform request: " + std::string(curl_easy_strerror(res)));
            }
            // check the status code
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            curl_easy_cleanup(curl);
            if (response_code >= 400)
            {
                throw std::runtime_error("Failed to fetch feed: " + std::to_string(response_code));
            }
            return parser;
        }

        void add_feeds(std::vector<std::string> urls, std::function<void(std::string_view)> error_sink, std::function<bool()> callback = []{ return true; })
        {
            std::thread([this, urls, callback, error_sink] {
                for (auto const &url_str : urls) {
                    try {
                        auto const ptr = add_feed_sync(url_str);
                        if (ptr->image_url().empty()) {
                            error_sink(std::format("Feed {}: no image found\n", url_str));
                        }
                    } 
                    catch(std::exception const &e) {
                        error_sink(std::format("Failed to add feed {}: {}\n", url_str, e.what()));
                    }
                    catch(...) {
                        error_sink(std::format("Failed to add feed {}\n", url_str));
                    }
                    if (!callback()) break;
                }
                })
                .detach();
        }

        auto add_feed_sync(std::string_view url)
        {
            auto feed_ptr = std::make_shared<rss::feed>(get_feed(url, system_runner_));
            {
                static std::mutex mutex;
                std::lock_guard<std::mutex> lock(mutex);
                auto feeds = feeds_.load();
                // does the feed already exist?
                auto pos = std::find_if(feeds->begin(), feeds->end(),
                                        [ourlink = feed_ptr->feed_link](auto const &f)
                                        {
                                            return f->feed_link == ourlink;
                                        });
                // add or replace with the new one
                if (pos != feeds->end())
                {
                    *pos = feed_ptr;
                }
                else
                {
                    feeds->emplace_back(feed_ptr);
                }
                // sort the feeds from the latest updated to the oldest
                std::sort(feeds->begin(), feeds->end(),
                          [](auto const &lhs, auto const &rhs)
                          {
                              return lhs->items.empty() ? false : (rhs->items.empty() ? true : lhs->items.front().updated > rhs->items.front().updated);
                          });
                feeds_.store(feeds);
            }
            return feed_ptr;
        }

        auto feeds()
        {
            return *feeds_.load();
        }

    private:
        std::atomic<std::shared_ptr<std::vector<std::shared_ptr<rss::feed>>>> feeds_ = std::make_shared<std::vector<std::shared_ptr<rss::feed>>>();
        std::function<std::string(std::string_view)> system_runner_;
    };
}