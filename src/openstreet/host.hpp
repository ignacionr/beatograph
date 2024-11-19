#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace openstreet {
    struct nominatim {
        static size_t write_to_string(void* ptr, size_t size, size_t nmemb, std::string* data) 
        {
            data->append((char*)ptr, size * nmemb);
            return size * nmemb;
        }

        static nlohmann::json get_location(const std::string& address) 
        {
            // split the address into components demarcated by commas
            std::vector<std::string> components;
            std::string component;
            for (const char& c : address) {
                if (c == ',') {
                    if(!component.empty()) components.push_back(component);
                    component.clear();
                } else {
                    component += c;
                }
            }
            if (!component.empty()) components.push_back(component);
            if (components.empty()) throw std::runtime_error("Invalid address format. Empty.");
            // reduce to the last two components e.g. "14 SomeStrasse, leave at the porter's, Berlin, Germany" -> "Berlin, Germany"
            if (components.size() > 2) {
                components.erase(components.begin(), components.end() - 2);
            }
            std::string location = components.size() > 1 ? components[components.size() - 2] + ", " + components[components.size() - 1] : components[0];
            // call nominatim API
            CURL* curl = curl_easy_init();
            if (!curl) throw std::runtime_error("Failed to initialize CURL.");
            std::string url = "https://nominatim.openstreetmap.org/search?q=" + location + "&format=json";
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
            std::string response;
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                curl_easy_cleanup(curl);
                throw std::runtime_error("Failed to perform CURL request.");
            }
            // return the location
            nlohmann::json json = nlohmann::json::parse(response);
            curl_easy_cleanup(curl);
            return json;
        }
    };
}