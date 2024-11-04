#pragma once

#include <map>
#include <stdexcept>
#include <string>

#include <curl/curl.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

namespace radio
{
    struct host
    {
        host() {}

        // Define a callback function to receive the audio data
        static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
            // Write the audio data to a buffer
            host *self = (host *)userdata;
            SDL_AudioDeviceID dev = self->dev;
            SDL_QueueAudio(dev, ptr, static_cast<Uint32>(size * nmemb));
            return size * nmemb;
        }

        void play_sync(std::string url)
        {
            // if the url is in the presets, use the preset
            if (presets.find(url) != presets.end())
            {
                url = presets[url];
            }

            // Initialize SDL
            SDL_Init(SDL_INIT_AUDIO);

            // Initialize libcurl
            CURL *curl;
            CURLcode res;
            curl_global_init(CURL_GLOBAL_DEFAULT);
            curl = curl_easy_init();
            if (curl)
            {
                // Set up the URL and other options
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

                // Initialize libavformat
                avformat_network_init();

                // Open the audio stream
                AVFormatContext *fmt_ctx = NULL;
                if (avformat_open_input(&fmt_ctx, url.c_str(), NULL, NULL) < 0)
                {
                    // Handle errors
                    throw std::runtime_error("Failed to open audio stream");
                }

                // Retrieve stream information
                auto ret = avformat_find_stream_info(fmt_ctx, nullptr);
                if (ret < 0) {
                    // Handle error
                    avformat_close_input(&fmt_ctx);
                    throw std::runtime_error("Failed to find stream information");
                }

                // Get the audio stream
                AVStream *stream = NULL;
                for (unsigned int i {0}; i < fmt_ctx->nb_streams; i++)
                {
                    if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                    {
                        stream = fmt_ctx->streams[i];
                        break;
                    }
                }

                if (!stream)
                {
                    // Handle errors
                }

                // Get the audio format
                AVCodecParameters *codecpar = stream->codecpar;
                int freq = codecpar->sample_rate;
                int channels = codecpar->ch_layout.nb_channels;
                int format = codecpar->format;

                // Convert the format to SDL format
                SDL_AudioFormat sdl_format = AUDIO_S16LSB;
                switch (format)
                {
                case AV_SAMPLE_FMT_S16:
                    sdl_format = AUDIO_S16LSB;
                    break;
                case AV_SAMPLE_FMT_S32:
                    sdl_format = AUDIO_S32LSB;
                    break;
                case AV_SAMPLE_FMT_FLT:
                    sdl_format = AUDIO_F32LSB;
                    break;
                case AV_SAMPLE_FMT_FLTP:
                    // Planar float format, convert to interleaved float format
                    sdl_format = AUDIO_F32LSB;
                    break;
                default:
                    throw std::runtime_error("Unsupported audio format");
                }

                // Initialize the audio device
                SDL_AudioSpec spec;
                spec.freq = freq;
                spec.format = sdl_format;
                spec.channels = static_cast<Uint8>(channels);
                spec.samples = 1024;
                spec.callback = NULL;
                spec.userdata = NULL;
                dev = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);

                // Set the callback function for the audio stream
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

                // Start playing the audio
                SDL_PauseAudioDevice(dev, 0);

                // Perform the request
                res = curl_easy_perform(curl);
                if (res != CURLE_OK)
                {
                    // Handle errors
                }

                // Clean up
                SDL_CloseAudioDevice(dev);
                curl_easy_cleanup(curl);
                avformat_close_input(&fmt_ctx);
            }
            curl_global_cleanup();

            // Clean up SDL
            SDL_Quit();
        }

        std::map<std::string, std::string> presets{
            {"Urbana Play", "https://cdn.instream.audio/:9660/stream"}};

    private:
        SDL_AudioDeviceID dev;
    };
}