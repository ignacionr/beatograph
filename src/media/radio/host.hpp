#pragma once

#include <algorithm>
#include <atomic>
#include <format>
#include <fstream>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>

#include <curl/curl.h>
extern "C"
{

#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
// #include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include <libavcodec/avcodec.h>
}
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include "../../registrar.hpp"
#include "../../structural/text_command/host.hpp"

namespace radio
{
    struct host
    {
        host(bool main_host = true)
        {
            if (main_host)
            {
                auto text_command_host = registrar::get<structural::text_command::host>({});
                text_command_host->add_command("Stop radio", [this]
                                               { stop(); });
                text_command_host->add_command("Play radio", [this]
                                               { play(last_played_); });

                // load the presets from file
                std::ifstream file("presets.txt");
                if (file.is_open())
                {
                    std::string line;
                    while (std::getline(file, line))
                    {
                        auto pos = line.find_first_of('=');
                        if (pos != std::string::npos)
                        {
                            presets_[line.substr(0, pos)] = line.substr(pos + 1);
                            text_command_host->add_command(std::format("Play {} on the radio", line.substr(0, pos)), [this, url = line.substr(pos + 1)]
                                                           { play(url); });
                        }
                    }
                }
            }
        }
        ~host() { stop(); }

        using release_audio_t = std::function<void()>;
        using audio_converter_t = std::function<release_audio_t(void *, uint32_t, void **, uint32_t *)>;

        void stop_and_wait()
        {
            stop();
            while (is_playing())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        void play(std::string url, std::chrono::milliseconds start = std::chrono::milliseconds{0})
        {
            static std::mutex play_mutex;
            std::lock_guard<std::mutex> lock(play_mutex);
            last_played_ = url;
            // start the player in a separate thread
            std::thread([this, url, start]()
                        {
                if (is_playing())
                {
                    stop_and_wait();
                }
                has_error_ = false;
                try {
                    play_sync(url, start);
                }
                catch(const std::exception &e) {
                    last_error_ = e.what();
                    has_error_ = true;
                } })
                .detach();
        }

        void stop()
        {
            std::lock_guard<std::mutex> lock(audio_device_mutex_);
            if (dev != 0)
            {
                SDL_PauseAudioDevice(dev, 1);
                SDL_ClearQueuedAudio(dev);
            }
            stream_metadata_.clear();
            update_stream_name();
            keep_playing = false;
        }

        static size_t ignore_write(void *, size_t size, size_t nmemb, void *)
        {
            return size * nmemb;
        };

        std::string resolve_redirections(std::string const &url)
        {
            CURL *curl = curl_easy_init();
            if (!curl)
            {
                throw std::runtime_error("Failed to initialize curl");
            }
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
            curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ignore_write);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, nullptr);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, nullptr);
            auto res = curl_easy_perform(curl);
            std::string result{url}; // if we fail, we return the original url
            if (res == CURLE_OK)
            {
                char *url_out;
                curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url_out);
            }
            curl_easy_cleanup(curl);
            return result;
        }

        void feed_the_beast(AVFormatContext *fmt_ctx, AVStream *audio_stream, AVCodecContext *codec_ctx, SwrContext *swr_ctx)
        {
            AVPacket packet;
            AVFrame *frame = av_frame_alloc();
            uint8_t *audio_buf = nullptr;

            playing = true;
            for (int read_result = av_read_frame(fmt_ctx, &packet); keep_playing.load(); read_result = av_read_frame(fmt_ctx, &packet))
            {
                if (read_result < 0)
                {
                    if (read_result == AVERROR_EOF)
                    {
                        std::cerr << "End of file" << std::endl;
                        break;
                    }
                    // obtain the error
                    char err[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(read_result, err, AV_ERROR_MAX_STRING_SIZE);
                    throw std::runtime_error(std::format("Failed to read frame, code: {}, {}", read_result, err));
                }
                if (packet.stream_index == audio_stream->index)
                {
                    auto const send_res = avcodec_send_packet(codec_ctx, &packet);
                    if (send_res >= 0)
                    {
                        for (;;)
                        {
                            auto res = avcodec_receive_frame(codec_ctx, frame);
                            if (res == AVERROR_EOF)
                            {
                                break;
                            }
                            else if (res == AVERROR(EAGAIN))
                            {
                                break;
                            }
                            else if (res < 0)
                            {
                                // obtain the error description
                                char err[AV_ERROR_MAX_STRING_SIZE];
                                av_strerror(res, err, AV_ERROR_MAX_STRING_SIZE);
                                throw std::runtime_error(std::format("Failed to receive frame, code: {}, {}", res, err));
                            }
                            int dst_nb_samples = static_cast<int>(av_rescale_rnd(swr_get_delay(swr_ctx, codec_ctx->sample_rate) + frame->nb_samples, codec_ctx->sample_rate, codec_ctx->sample_rate, AV_ROUND_UP));
                            int dst_buf_size = av_samples_get_buffer_size(nullptr, obtained_spec.channels, dst_nb_samples, AV_SAMPLE_FMT_FLT, 1);
                            audio_buf = (uint8_t *)av_malloc(dst_buf_size);
                            auto converted = swr_convert(swr_ctx, &audio_buf, dst_nb_samples, (const uint8_t **)frame->extended_data, frame->nb_samples);
                            if (converted < 0)
                            {
                                av_free(audio_buf);
                                throw std::runtime_error("Failed to convert audio");
                            }
                            // update the current packet sent
                            if (current_packet_length_.count() == 0)
                            {
                                current_packet_sent_ = std::chrono::system_clock::now();
                            }
                            current_packet_length_ += std::chrono::milliseconds{1000 * frame->nb_samples / codec_ctx->sample_rate};
                            auto sdl_ret = SDL_QueueAudio(dev, audio_buf, dst_buf_size);
                            if (sdl_ret < 0)
                            {
                                av_free(audio_buf);
                                throw std::runtime_error("Failed to queue audio");
                            }

                            av_free(audio_buf);
                        }
                    }
                    else
                    {
                        std::cerr << std::format("Failed to send packet to codec, code: {}\n", send_res);
                    }
                }
                else
                {
                    std::cerr << std::format("Skipping packet from stream {}\n", packet.stream_index);
                }
                av_packet_unref(&packet);
            }
            av_frame_free(&frame);
        }

        static void init()
        {
            // Initialize SDL
            if (SDL_Init(SDL_INIT_AUDIO) < 0)
            {
                throw std::runtime_error("Failed to initialize SDL");
            }
        }

        void play_sync(std::string url, std::chrono::milliseconds start = std::chrono::milliseconds{0})
        {
            keep_playing = true;
            // if the url is in the presets, use the preset
            if (presets_.find(url) != presets_.end())
            {
                url = presets_[url];
            }
            else
            {
                url = resolve_redirections(url);
            }

            // Open the audio stream
            AVFormatContext *fmt_ctx = nullptr;
            if (auto res = avformat_open_input(&fmt_ctx, url.c_str(), nullptr, nullptr); res < 0)
            {
                // obtain the error description
                char err[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(res, err, AV_ERROR_MAX_STRING_SIZE);
                throw std::runtime_error(std::format("Failed to open audio stream: {} {}", err, url));
            }

            // Retrieve stream information
            if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
            {
                avformat_close_input(&fmt_ctx);
                throw std::runtime_error(std::format("Failed to find stream information: {}", url));
            }

            // Retrieve the name of the stream
            stream_metadata_.clear();
            if (fmt_ctx->metadata)
            {
                const AVDictionaryEntry *e = nullptr;
                while ((e = av_dict_iterate(fmt_ctx->metadata, e)))
                {
                    stream_metadata_[e->key] = e->value;
                }
            }
            update_stream_name();

            // Get the audio stream
            AVStream *audio_stream = nullptr;
            for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
            {
                if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                {
                    audio_stream = fmt_ctx->streams[i];
                    break;
                }
            }

            if (!audio_stream)
            {
                avformat_close_input(&fmt_ctx);
                throw std::runtime_error("No audio stream found");
            }

            // Find the decoder for the audio stream
            auto codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
            if (!codec)
            {
                avformat_close_input(&fmt_ctx);
                throw std::runtime_error("Failed to find audio codec");
            }

            // Allocate a codec context for the decoder
            AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
            if (!codec_ctx)
            {
                avformat_close_input(&fmt_ctx);
                throw std::runtime_error("Failed to allocate codec context");
            }

            // Copy codec parameters from input stream to output codec context
            if (avcodec_parameters_to_context(codec_ctx, audio_stream->codecpar) < 0)
            {
                avcodec_free_context(&codec_ctx);
                avformat_close_input(&fmt_ctx);
                throw std::runtime_error("Failed to copy codec parameters to codec context");
            }

            // Initialize the codec context to use the given codec
            if (avcodec_open2(codec_ctx, codec, nullptr) < 0)
            {
                avcodec_free_context(&codec_ctx);
                avformat_close_input(&fmt_ctx);
                throw std::runtime_error("Failed to open codec");
            }

            if (dev == 0)
            {
                std::lock_guard<std::mutex> lock(audio_device_mutex_);
                // Set up SDL audio specs
                SDL_AudioSpec wanted_spec{};
                wanted_spec.freq = codec_ctx->sample_rate;
                wanted_spec.format = AUDIO_F32SYS;
                wanted_spec.channels = static_cast<uint8_t>(codec_ctx->ch_layout.nb_channels);
                wanted_spec.silence = 0;
                wanted_spec.samples = 1024;
                wanted_spec.callback = nullptr;
                dev = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &obtained_spec, 0);
                if (dev == 0)
                {
                    avcodec_free_context(&codec_ctx);
                    avformat_close_input(&fmt_ctx);
                    throw std::runtime_error("Failed to open audio device");
                }
            }

            // Initialize the resampler
            SwrContext *swr_ctx = swr_alloc();
            av_opt_set_int(swr_ctx, "in_channel_count", codec_ctx->ch_layout.nb_channels, 0);
            av_opt_set_int(swr_ctx, "in_sample_rate", codec_ctx->sample_rate, 0);
            av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", codec_ctx->sample_fmt, 0);
            av_opt_set_int(swr_ctx, "out_channel_count", obtained_spec.channels, 0);
            av_opt_set_int(swr_ctx, "out_sample_rate", obtained_spec.freq, 0);
            av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
            auto ret = swr_init(swr_ctx);
            if (ret < 0)
            {
                avcodec_free_context(&codec_ctx);
                avformat_close_input(&fmt_ctx);
                throw std::runtime_error("Failed to initialize resampler");
            }

            // determine the total length as a duration
            auto duration = fmt_ctx->duration;
            if (duration != AV_NOPTS_VALUE)
            {
                duration /= AV_TIME_BASE;
            }
            total_run_ = std::chrono::milliseconds{1000 * duration};
            current_packet_length_ = std::chrono::milliseconds{0};

            if (start.count() > 0)
            {
                // seek to the start
                if (0 <= av_seek_frame(fmt_ctx, -1, start.count() * AV_TIME_BASE / 1000, 0))
                {
                    current_run_ = start;
                }
                else
                {
                    current_run_ = std::chrono::milliseconds{0};
                }
            }
            else
            {
                current_run_ = std::chrono::milliseconds{0};
            }

            // Start playing audio
            SDL_PauseAudioDevice(dev, 0);

            last_played_ = url;
            feed_the_beast(fmt_ctx, audio_stream, codec_ctx, swr_ctx);

            // Clean up
            swr_free(&swr_ctx);
            avcodec_free_context(&codec_ctx);
            avformat_close_input(&fmt_ctx);
            while (SDL_GetQueuedAudioSize(dev) > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::lock_guard<std::mutex> lock(audio_device_mutex_);
            SDL_CloseAudioDevice(dev);
            dev = 0;
            playing = false;
        }

        bool is_playing() const
        {
            return playing.load();
        }

        auto presets() const
        {
            // return the preset keys
            std::vector<std::string> keys;
            std::transform(presets_.begin(), presets_.end(), std::back_inserter(keys),
                           [](const auto &preset)
                           { return preset.first; });
            return keys;
        }

        auto has_error() const
        {
            return has_error_.load();
        }
        auto last_error() const
        {
            return last_error_;
        }

        std::optional<std::string> last_played() const
        {
            if (!playing || last_played_.empty())
            {
                return std::nullopt;
            }
            return last_played_;
        }

        std::string const &stream_name() const {
            return *current_stream_name_.load();
        }

        void update_stream_name()
        {
            if (stream_metadata_.find("title") != stream_metadata_.end())
            {
                current_stream_name_.store(std::make_shared<std::string>(stream_metadata_.at("title")));
            }
            else if (stream_metadata_.find("icy-name") != stream_metadata_.end())
            {
                current_stream_name_.store(std::make_shared<std::string>(stream_metadata_.at("icy-name")));
            }
            else if (!stream_metadata_.empty())
            {
                auto it = stream_metadata_.begin();
                current_stream_name_.store(std::make_shared<std::string>(std::format("{}: {}", it->first, it->second)));
            }
            else
            {
                current_stream_name_.store(std::make_shared<std::string>());
            }
        }

        std::chrono::milliseconds total_run() const
        {
            return total_run_;
        }

        std::chrono::milliseconds current_run() const
        {
            auto now = std::chrono::system_clock::now();
            if (current_packet_length_.count() == 0)
            {
                return current_run_;
            }
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - current_packet_sent_);
            if (diff > current_packet_length_)
            {
                diff = current_packet_length_;
            }
            return current_run_ + diff;
        }

        void move_to(std::chrono::milliseconds new_position)
        {
            stop_and_wait();
            play(last_played_, new_position);
        }

    private:
        std::map<std::string, std::string> presets_;

        std::mutex audio_device_mutex_;
        std::atomic<std::shared_ptr<std::string>> current_stream_name_{std::make_shared<std::string>()};
        SDL_AudioDeviceID dev{};
        SDL_AudioSpec obtained_spec{};
        std::atomic<bool> playing{false};
        std::atomic<bool> keep_playing{true};
        std::atomic<float> level{0.0f};
        std::atomic<bool> has_error_{false};
        std::string last_error_;
        std::string last_played_;
        std::unordered_map<std::string, std::string> stream_metadata_;
        std::chrono::milliseconds total_run_{};
        std::chrono::milliseconds current_run_{};
        std::chrono::milliseconds current_packet_length_{};
        std::chrono::system_clock::time_point current_packet_sent_{};
    };
}