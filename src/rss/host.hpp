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
        host() {
            add_feeds( {
                "https://ankar.io/this-american-life-archive/TALArchive.xml" // This American Life
                , "https://cppcast.com/feed.rss" // cppcast
                , "https://feeds.twit.tv/brickhouse.xml"
                , "https://feeds.twit.tv/twit.xml"
                , "https://softwareengineeringdaily.com/feed/podcast/"
                , "https://www.spreaker.com/show/2576750/episodes/feed" // compute this
                , "https://www.spreaker.com/show/3392139/episodes/feed" // Internet Freakshows
                , "https://www.spreaker.com/show/4209606/episodes/feed" // los temas del día
                , "https://www.spreaker.com/show/4956890/episodes/feed" // CP radio
                , "https://www.spreaker.com/show/5634793/episodes/feed" // dotnet rocks
                , "https://www.spreaker.com/show/5711490/episodes/feed" // working class history
                , "https://www.spreaker.com/show/5719641/episodes/feed" // the digital decode
                , "https://www.spreaker.com/show/6006838/episodes/feed" // cine para pensar
                , "https://www.spreaker.com/show/6102036/episodes/feed" // adventures in DevOps
                , "https://www.spreaker.com/show/6332631/episodes/feed" // cultura líquida
                , "https://www.spreaker.com/show/6349862/episodes/feed" // Llama Cast
                , "https://feeds.transistor.fm/energy-bytes" // Energy Bytes ?
                , "https://feeds.transistor.fm/techradio"
                , "https://rss.podplaystudio.com/2743.xml" // Tech Talk with Jess Kelly
                , "https://feeds.simplecast.com/Sl5CSM3S"
                , "https://sbs-ondemand.streamguys1.com/sbs-russian/"
                , "https://changelog.com/podcast/feed"
                , "http://feeds.feedburner.com/VenganzasDelPasado"
                , "https://feeds.transistor.fm/test-and-code"
                , "https://feeds.npr.org/510351/podcast.xml"
                , "https://rss.art19.com/the-lead"
                , "https://feeds.transistor.fm/small-big-wins"
                , "https://feeds.megaphone.fm/BVLLC2163264914"
                , "https://feeds.simplecast.com/Sl5CSM3S"
                , "https://www.omnycontent.com/d/playlist/e73c998e-6e60-432f-8610-ae210140c5b1/a91018a4-ea4f-4130-bf55-ae270180c327/44710ecc-10bb-48d1-93c7-ae270180c33e/podcast.rss"
                , "https://allinchamathjason.libsyn.com/rss"
                , "https://feeds.simplecast.com/hNaFxXpO"
                , "https://rss.pdrl.fm/55dc8e/feeds.megaphone.fm/WWO7410387571"
                , "https://feeds.simplecast.com/WCb5SgYj"
                , "https://audioboom.com/channels/5021027.rss"
                , "https://audioboom.com/channels/5021027.rss"
                , "https://anchor.fm/s/28fef6f0/podcast/rss"
                , "https://anchor.fm/s/f35c22c4/podcast/rss"
                , "https://feed.podbean.com/podcast.hernancattaneo.com/feed.xml"
                , "https://feeds.megaphone.fm/ADSMOVILESPAASL6591275552"
                , "https://anchor.fm/s/5e295028/podcast/rss"
                , "https://feeds.megaphone.fm/SONORO5005023613"
                , "https://www.spreaker.com/show/4725236/episodes/feed"
                , "https://podcasts.files.bbci.co.uk/p02nq0gn.rss"
                , "https://www.spreaker.com/show/4731320/episodes/feed"
                , "https://anchor.fm/s/4cc7fadc/podcast/rss"
                , "https://cuonda.com/monos-estocasticos/feed"
                , "https://feeds.npr.org/510355/podcast.xml"
                , "https://anchor.fm/s/5936c94c/podcast/rss"
                , "https://fapi-top.prisasd.com/podcast/elpais/las_noticias_del_pais.xml"
                , "https://video-api.wsj.com/podcast/rss/wsj/whats-news"
                , "https://www.spreaker.com/show/5718542/episodes/feed"
                , "https://mavecloud.s3mts.ru/storage/feeds/41353.xml"
                , "https://feeds.transistor.fm/0b18f927-efcb-4ed8-87c4-3cf9642a0de6"});
        }

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

        void add_feeds(std::vector<std::string> urls, std::function<void()> callback = {}) {
            std::thread([this, urls, callback] {
                try {
                    for (auto const &url_str : urls) {
                        add_feed_sync(url_str);
                    }
                    if (callback) callback();
                }
                catch(...) {
                    // ignore
                }
            }).detach();
        }

        void add_feed_sync(std::string_view url) {
            auto feed_ptr = std::make_shared<rss::feed>(get_feed(url));
            {
                static std::mutex mutex;
                std::lock_guard<std::mutex> lock(mutex);
                auto feeds = feeds_.load();
                // does the feed already exist?
                auto pos = std::find_if(feeds->begin(), feeds->end(), 
                    [ourlink = feed_ptr->feed_link](auto const &f) {
                        return f->feed_link == ourlink;
                });
                // add or replace with the new one
                if (pos != feeds->end()) {
                    *pos = feed_ptr;
                }
                else {
                    feeds->emplace_back(std::move(feed_ptr));
                }
                feeds_.store(feeds);
            }
        }

        auto feeds() {
            return *feeds_.load();
        }
    private:
        std::atomic<std::shared_ptr<std::vector<std::shared_ptr<rss::feed>>>> feeds_ = std::make_shared<std::vector<std::shared_ptr<rss::feed>>>();
    };
}