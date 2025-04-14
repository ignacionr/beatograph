#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <curl/curl.h>
#include <GL/glew.h>

#if defined (SUPPORT_SVG)
extern "C"
{
#include <librsvg/rsvg.h>
}
#endif

#include "hosting/http/fetch.hpp"

struct img_cache
{
    img_cache(std::filesystem::path cache_path) : cache_path_{cache_path}
    {
        if (!std::filesystem::exists(cache_path_))
        {
            std::filesystem::create_directories(cache_path_);
        }
        // is there an index.txt file?
        auto index_path = cache_path_ / "index.txt";
        if (std::filesystem::exists(index_path))
        {
            std::ifstream index_file{index_path};
            std::string line;
            while (std::getline(index_file, line))
            {
                auto pos = line.find(' ');
                if (pos != std::string::npos)
                {
                    index_[line.substr(0, pos)] = line.substr(pos + 1);
                }
            }
        }
    }
    ~img_cache()
    {
        try
        {
            save();
        }
        catch (...)
        {
            // ignore, after all this is a destructor
        }
    }

    void save()
    {
        std::ofstream index_file{cache_path_ / "index.txt"};
        std::lock_guard lock{index_mutex_};
        for (auto const &[key, value] : index_)
        {
            index_file << key << ' ' << value << '\n';
        }
    }

    static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        auto image_file = static_cast<std::ofstream *>(userp);
        image_file->write(static_cast<char *>(contents), size * nmemb);
        return size * nmemb;
    }

    long long load_texture_from_file(std::string const &file_path)
    {
        if (auto const pos{textures_.find(file_path)}; pos != textures_.end())
        {
            return pos->second;
        }
        SDL_Surface *surface = nullptr;
#if defined (SUPPORT_SVG)
        if (file_path.ends_with(".SVG") || file_path.ends_with(".svg"))
        {
            // Initialize librsvg
            RsvgHandle *handle = rsvg_handle_new_from_file(file_path.c_str(), nullptr);
            if (!handle)
            {
                throw std::runtime_error(std::format("Failed to load SVG: {}", file_path));
            }

            // Get the dimensions of the SVG
            RsvgDimensionData dimensions;
            rsvg_handle_get_dimensions(handle, &dimensions);

            // Create an SDL surface with the dimensions of the SVG
            surface = SDL_CreateRGBSurfaceWithFormat(0, dimensions.width, dimensions.height, 32, SDL_PIXELFORMAT_RGBA32);
            if (!surface)
            {
                g_object_unref(handle);
                throw std::runtime_error(std::format("Failed to create SDL surface for SVG: {}", SDL_GetError()));
            }

            // Render the SVG onto the SDL surface
            cairo_surface_t *cairo_surface = cairo_image_surface_create_for_data(
                static_cast<unsigned char *>(surface->pixels),
                CAIRO_FORMAT_ARGB32,
                surface->w,
                surface->h,
                surface->pitch);
            cairo_t *cr = cairo_create(cairo_surface);
            rsvg_handle_render_cairo(handle, cr);
            cairo_destroy(cr);
            cairo_surface_destroy(cairo_surface);
            g_object_unref(handle);
        }
        else
#endif // SUPPORT_SVG
        {
            surface = IMG_Load(file_path.c_str());
            if (!surface)
            {
                throw std::runtime_error(std::format("Failed to load image: {}", IMG_GetError()));
            }
            // Convert the surface to a standard format
            SDL_Surface *converted_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(surface);
            if (!converted_surface)
            {
                throw std::runtime_error(std::format("Failed to convert surface: {}", SDL_GetError()));
            }
            surface = converted_surface;
        }

        unsigned int texture{0};
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        GLint internal_format{GL_RGBA};
        GLenum format{GL_RGBA};
        GLenum type{GL_UNSIGNED_BYTE};
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, surface->w, surface->h, 0, format, type, surface->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        SDL_FreeSurface(surface);
        textures_[file_path] = texture;
        return texture;
    }

    long long load_texture_from_url(std::string const &url, 
        http::fetch::header_client_t header_client = {}, 
        std::string const &default_extension = ".png",
        std::optional<std::chrono::minutes> max_age = std::nullopt)
    {
        std::lock_guard lock{index_mutex_};
        auto it = index_.find(url);
        if (it != index_.end())
        {
            return load_texture_from_file(it->second);
        }
        // is there a thread already downloading this?
        auto it_thread = loader_threads_.find(url);
        if (it_thread == loader_threads_.end())
        {
            // create one
            loader_threads_[url] = std::jthread{
                [this, url, header_client, default_extension, max_age]
                {
                    try
                    {
                        load_into_cache(url, header_client, default_extension, max_age);
                    }
                    catch (std::exception const &e)
                    {
                        std::cerr << "Failed to load image: " << e.what() << '\n';
                        // and ignore, we will just show a placeholder forever
                    }
                }};
        }
        // still loading, return a placeholder
        return default_texture();
    }

    long long default_texture() {
        return load_texture_from_file("assets/b6a9d081425dd6a.png");
    }

    void load_into_cache(std::string const &url, http::fetch::header_client_t header_client, std::string_view default_extension, std::optional<std::chrono::minutes> max_age)
    {
        // obtain the url path plus filename part only (exclude querystring or hash)
        auto const pos_qs = url.find('?');
        auto const pos_hash = url.find('#');
        auto const pos_end = pos_qs < pos_hash ? pos_qs : pos_hash;
        auto const url_path = url.substr(0, pos_end);

        // get the url extension
        auto pos = url_path.find_last_of('.');
        std::string extension;
        if (pos != std::string::npos)
        {
            extension = url_path.substr(pos);
        }
        if (extension.size() > 5)
        {
            extension.clear();
        }
        if (extension.empty())
        {
            extension = default_extension;
        }
        // make the extension uppercase
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c)
                       { return std::toupper(c); });
        auto file_path = cache_path_ / std::format("{}{}", std::hash<std::string>{}(url), extension);

        if (!std::filesystem::exists(file_path) || 
            (max_age.has_value() && std::filesystem::last_write_time(file_path) < std::filesystem::file_time_type::clock::now() - *max_age))
        {
            // download the image into file_path
            try {
                http::fetch fetcher;
                std::ofstream image_file{file_path, std::ios::binary};
                fetcher(url, header_client, write_callback, &image_file);
            }
            catch (std::exception const &)
            {
                std::filesystem::remove(file_path);
                throw;
            }
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
    std::map<std::string, long long> textures_;
};
