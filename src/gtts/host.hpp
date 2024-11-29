#pragma once

#include <algorithm>
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

        void tts_to_file(std::string_view text, const std::string &file_name, std::string_view lang = "en-US") {
            auto url = std::format("https://translate.google.com/translate_tts?ie=UTF-8&tl={}&client=tw-ob&q={}", lang, text);
            CURL *curl = curl_easy_init();
            if (curl) {
                FILE *file = nullptr;
                if (fopen_s(&file, file_name.c_str(), "wb") == 0 && file) {
                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
                    auto res = curl_easy_perform(curl);
                    fclose(file);
                    if (res != CURLE_OK) {
                        curl_easy_cleanup(curl);
                        throw std::runtime_error(std::format("Failed to download audio: {}", curl_easy_strerror(res)));
                    }
                }
                else {
                    throw std::runtime_error("Failed to open file for writing");
                }
                curl_easy_cleanup(curl);
            }
        }

        std::string tts_to_cache(std::string_view text, std::string_view lang = "en-US") {
            auto const text_as_uri_component {conversions::uri::encode_component(text)};
            auto const cache_key {std::format("{}-{}", lang, text_as_uri_component)};
            auto it = cache_.find(cache_key);
            if (it != cache_.end()) {
                return it->second;
            }
            auto local_name = std::format("{}.mp3", cache_.size());
            auto file_name = (cache_path_ / local_name).string();
            tts_to_file(text_as_uri_component, file_name, lang);
            cache_[cache_key] = file_name;
            return file_name;
        }

        void tts_job(std::string_view text, std::function<void(std::string_view file_produced)> sink, std::string_view lang = "en-US") {
            std::vector<std::string> file_names;
            // cut down to word boundaries up to 200 characters
            size_t slice_size;
            for (std::string_view slice = text; !slice.empty(); slice = slice.substr(slice_size)) {
                slice_size = std::min(slice.size(), 200ull);
                if (slice_size == 200ull) {
                    while (std::isalpha(slice[slice_size - 1])|| std::isdigit(slice[slice_size - 1])) {
                        --slice_size;
                    }
                }
                auto slice_text = slice.substr(0, slice_size);
                auto file_name = tts_to_cache(slice_text, lang);
                file_names.push_back(file_name);
            }
            for (auto const &file_name : file_names) {
                sink(file_name);
            }
        }

    private:
        std::filesystem::path cache_path_;
        std::unordered_map<std::string, std::string> cache_;
    };
}
