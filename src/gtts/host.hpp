#pragma once

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <unordered_map>

#include <curl/curl.h>
#include "../conversions/uri.hpp"

namespace gtts {
    struct host {
        host(std::filesystem::path cache_path): cache_path_{cache_path} {
            std::ifstream file{cache_path_ / "gtts.cache"};
            if (file) {
                std::string line;
                while (std::getline(file, line)) {
                    auto pos = line.find_first_of(' ');
                    if (pos != std::string::npos) {
                        cache_[line.substr(0, pos)] = line.substr(pos + 1);
                    }
                }
            }
        }
        ~host() {
            save();
        }

        host(const host &) = delete;

        void save() {
            std::ofstream file{cache_path_ / "gtts.cache"};
            for (auto const &entry : cache_) {
                file << entry.first << ' ' << entry.second << '\n';
            }
        }

        void tts_to_file(std::string_view text, const std::string &file_name) {
            auto url = std::format("https://translate.google.com/translate_tts?ie=UTF-8&tl=en-US&client=tw-ob&q={}", text);
            CURL *curl = curl_easy_init();
            if (curl) {
                FILE *file = nullptr;
                if (fopen_s(&file, file_name.c_str(), "wb") == 0 && file) {
                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
                    curl_easy_perform(curl);
                    fclose(file);
                }
                else {
                    throw std::runtime_error("Failed to open file for writing");
                }
                curl_easy_cleanup(curl);
            }
        }

        std::string tts_to_cache(std::string_view text) {
            auto const text_as_uri_component {conversions::uri::encode_component(text)};
            auto it = cache_.find(text_as_uri_component);
            if (it != cache_.end()) {
                return it->second;
            }
            auto local_name = std::format("{}.mp3", cache_.size());
            auto file_name = (cache_path_ / local_name).string();
            tts_to_file(text_as_uri_component, file_name);
            cache_[text_as_uri_component] = file_name;
            return file_name;
        }

    private:
        std::filesystem::path cache_path_;
        std::unordered_map<std::string, std::string> cache_;
    };
}
