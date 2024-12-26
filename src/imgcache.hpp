#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <curl/curl.h>
#include <GL/glew.h>


struct img_cache {
    img_cache(std::filesystem::path cache_path): cache_path_{cache_path} {
        if (!std::filesystem::exists(cache_path_)) {
            std::filesystem::create_directories(cache_path_);
        }
        // is there an index.txt file?
        auto index_path = cache_path_ / "index.txt";
        if (std::filesystem::exists(index_path)) {
            std::ifstream index_file{index_path};
            std::string line;
            while (std::getline(index_file, line)) {
                auto pos = line.find(' ');
                if (pos != std::string::npos) {
                    index_[line.substr(0, pos)] = line.substr(pos + 1);
                }
            }
        }
    }
    ~img_cache() {
        try {
            save();
        }
        catch(...) {
            // ignore, after all this is a destructor
        }
    }

    void save() {
        std::ofstream index_file{cache_path_ / "index.txt"};
        std::lock_guard lock{index_mutex_};
        for (auto const &[key, value] : index_) {
            index_file << key << ' ' << value << '\n';
        }
    }

    static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
        auto image_file = static_cast<std::ofstream *>(userp);
        image_file->write(static_cast<char *>(contents), size * nmemb);
        return size * nmemb;
    }

    unsigned int load_texture_from_file(std::string const &file_path) {
        if (auto const pos {textures_.find(file_path)}; pos != textures_.end()) {
            return pos->second;
        }
        auto surface = IMG_Load(file_path.c_str());
        if (!surface) {
            throw std::runtime_error(std::format("Failed to load image: {}", IMG_GetError()));
        }
        unsigned int texture{0};
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        GLint internal_format{surface->format->BytesPerPixel == 4 ? GL_RGBA : GL_RGB};
        GLenum format{surface->format->BytesPerPixel == 4 ? static_cast<GLenum>(GL_RGBA) : static_cast<GLenum>(GL_RGB)};
        GLenum type{GL_UNSIGNED_BYTE};
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, surface->w, surface->h, 0, format, type, surface->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        SDL_FreeSurface(surface);
        textures_[file_path] = texture;
        return texture;
    }

    unsigned int load_texture_from_url(std::string const &url) {
        std::lock_guard lock{index_mutex_};
        auto it = index_.find(url);
        if (it != index_.end()) {
            return load_texture_from_file(it->second);
        }
        // is there a thread already downloading this?
        auto it_thread = loader_threads_.find(url);
        if (it_thread == loader_threads_.end()) {
            // craete one
            loader_threads_[url] = std::jthread{
                [this, url] {
                    try {
                        load_into_cache(url);
                    }
                    catch (std::exception const &e) {
                        std::cerr << "Failed to load image: " << e.what() << '\n';
                        // and ignore, we will just show a placeholder forever
                    }
                }};
        }
        // still loading, return a placeholder
        return load_texture_from_file("assets/b6a9d081425dd6a.png");
    }

    void load_into_cache(std::string const &url) {
        // obtain the url path plus filename part only (exclude querystring or hash)
        auto const pos_qs = url.find('?');
        auto const pos_hash = url.find('#');
        auto const pos_end = std::min(pos_qs, pos_hash);
        auto const url_path = url.substr(0, pos_end);

        // get the url extension
        auto pos = url_path.find_last_of('.');
        std::string extension;
        if (pos != std::string::npos) {
            extension = url_path.substr(pos);
        }
        if (extension.size() > 5) {
            extension.clear();
        }
        // make the extension uppercase
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::toupper(c); });
        auto file_path = cache_path_ / std::format("{}{}", std::hash<std::string>{}(url), extension);

        if (!std::filesystem::exists(file_path)) {

            // download the image into file_path
            CURL *curl = curl_easy_init();
            if (!curl) {
                throw std::runtime_error("Failed to initialize curl");
            }
            {
                std::ofstream image_file{file_path, std::ios::binary};
                if (!image_file) {
                    throw std::runtime_error(std::format("Failed to open file: {}", file_path.string()));
                }
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
                curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
                curl_easy_setopt(curl, CURLOPT_USERAGENT, "beat-o-graph/1.0");
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &image_file);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                auto res = curl_easy_perform(curl);
                if (res != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    image_file.close();
                    std::filesystem::remove(file_path);
                    throw std::runtime_error(std::format("Failed to download image: {}", curl_easy_strerror(res)));
                }
                // get the status code
                long http_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                if (http_code != 200) {
                    curl_easy_cleanup(curl);
                    image_file.close();
                    std::filesystem::remove(file_path);
                    throw std::runtime_error(std::format("Failed to download image: HTTP status code {}", http_code));
                }
            }
            curl_easy_cleanup(curl);
        }
        {
            std::lock_guard lock{index_mutex_};
            index_[url] = file_path.string();
        }
    }

private:
    std::filesystem::path cache_path_;
    std::map<std::string, std::string> index_;
    std::mutex index_mutex_;
    std::map<std::string, std::jthread> loader_threads_;
    std::map<std::string, unsigned int> textures_;
};
