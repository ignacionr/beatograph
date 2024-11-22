#pragma once

#include <string>

#include <curl/curl.h>

namespace gtts {
    struct host {
        host() = default;
        host(const host &) = delete;

        void tts_to_file(std::string_view text, std::string_view file_name) {
            CURL *curl;
            CURLcode res;
            curl = curl_easy_init();
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, "https://api.sttapi.com/tts");
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, text.data());
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, file_name.data());
                res = curl_easy_perform(curl);
                if (res != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    throw std::runtime_error("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
                }
                curl_easy_cleanup(curl);
            }
        }


        std::string base_url =
            "https://translate.google.com/translate_tts?ie=UTF-8&q=";
        std::string lang_ = "&tl=";
        std::string text_ = "";
        std::string client_ = "&client=tw-ob' ";
        std::string out_ = "> /tmp/gtts.mp3";
        std::string outv_ = "> /tmp/gtts_"; 
        std::string ref_ = " 'Referer: http://translate.google.com/' ";
        std::string agent_ = " 'User-Agent: stagefright/1.2 (Linux;Android 9.0)' ";
        std::string mpv_ = "mpv";
        std::string speed_ = " --speed=";
        std::string play_ = " /tmp/gtts.mp3 1>/dev/null";
        std::string cat_ = "cat /tmp/gtts_*.mp3 > /tmp/gtts.mp3";
        std::string rm_ = "rm /tmp/gtts_*.mp3";
    };
}