#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <string>

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
        save();
    }

    void save() const {
        std::ofstream index_file{cache_path_ / "index.txt"};
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
        auto it = index_.find(url);
        if (it != index_.end()) {
            return load_texture_from_file(it->second);
        }
        // get the url extension
        auto pos = url.find_last_of('.');
        if (pos == std::string::npos) {
            throw std::runtime_error("Failed to determine the extension of the image");
        }
        auto extension = url.substr(pos);
        // make the extension uppercase
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::toupper(c); });
        auto file_path = cache_path_ / std::format("{}{}", std::hash<std::string>{}(url), extension);

        // download the image into file_path
        CURL *curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize curl");
        }
        {
            std::ofstream image_file{file_path, std::ios::binary};
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &image_file);
            auto res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                curl_easy_cleanup(curl);
                throw std::runtime_error(std::format("Failed to download image: {}", curl_easy_strerror(res)));
            }
        }
        curl_easy_cleanup(curl);
        index_[url] = file_path.string();
        return load_texture_from_file(file_path.string());
    }

private:
    std::filesystem::path cache_path_;
    std::map<std::string, std::string> index_;
    std::map<std::string, unsigned int> textures_;
};
