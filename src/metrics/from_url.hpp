#pragma once

#include "metrics_model.hpp"
#include "metrics_parser.hpp"

#include <curl/curl.h>

struct metrics_from_url {
    static metrics_model fetch(std::string_view url) 
    {
        metrics_model model;
        metrics_parser parser;
        parser.metric_help = [&model](std::string_view name, std::string_view help) {
            model.set_help(name, help);
        };
        parser.metric_metric_value = [&model](std::string_view name, metric_value&& value) {
            model.add_value(name, std::move(value));
        };
        parser.metric_type = [&model](std::string_view name, std::string_view type) {
            model.set_type(name, type);
        };
        // perform request
        CURL *curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize curl");
        }
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Accept: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_URL, url.data());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &parser);
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) {
            throw std::runtime_error("Failed to perform request: " + std::string(curl_easy_strerror(res)));
        }
        return model;
    }
private:
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp) 
    {
        auto parser = static_cast<metrics_parser*>(userp);
        (*parser)(std::string_view{static_cast<char*>(contents), size * nmemb});
        return size * nmemb;
    }
};
