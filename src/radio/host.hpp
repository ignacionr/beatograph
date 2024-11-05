#pragma once

#include <algorithm>
#include <atomic>
#include <format>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>

#include <curl/curl.h>
extern "C" {

#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include <libavcodec/avcodec.h>
}
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

namespace radio
{
    struct host
    {
        host() {}

        using release_audio_t = std::function<void()>;
        using audio_converter_t = std::function<release_audio_t(void *, uint32_t, void **, uint32_t *)>;

        void play(std::string url) {
            std::thread([this, url]() {
                if (is_playing())
                {
                    stop();
                    while (is_playing())
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
                has_error_ = false;
                try {
                    play_sync(url);
                }
                catch(const std::exception &e) {
                    last_error_ = e.what();
                    has_error_ = true;
                }
            }).detach();
        }

        void stop() {
            keep_playing = false;
        }

        void play_sync(std::string url)
        {
            keep_playing = true;
            // if the url is in the presets, use the preset
            if (presets_.find(url) != presets_.end())
            {
                url = presets_[url];
            }

            // Initialize SDL
            if (SDL_Init(SDL_INIT_AUDIO) < 0)
            {
                throw std::runtime_error("Failed to initialize SDL");
            }

            // Initialize libavformat
            avformat_network_init();

            // Open the audio stream
            AVFormatContext *fmt_ctx = nullptr;
            if (avformat_open_input(&fmt_ctx, url.c_str(), nullptr, nullptr) < 0)
            {
                throw std::runtime_error(std::format("Failed to open audio stream: {}", url));
            }

            // Retrieve stream information
            if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
            {
                avformat_close_input(&fmt_ctx);
                throw std::runtime_error(std::format("Failed to find stream information: {}", url));
            }

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

            // Set up SDL audio specs
            SDL_AudioSpec wanted_spec, obtained_spec;
            wanted_spec.freq = codec_ctx->sample_rate;
            wanted_spec.format = AUDIO_F32SYS;
            wanted_spec.channels = static_cast<uint8_t>(codec_ctx->ch_layout.nb_channels);
            wanted_spec.silence = 0;
            wanted_spec.samples = 1024;
            wanted_spec.callback = nullptr;

            if (SDL_OpenAudio(&wanted_spec, &obtained_spec) < 0)
            {
                avcodec_free_context(&codec_ctx);
                avformat_close_input(&fmt_ctx);
                throw std::runtime_error("Failed to open audio device");
            }

            // Initialize the resampler
            SwrContext *swr_ctx = swr_alloc();
            av_opt_set_int(swr_ctx, "in_channel_count", codec_ctx->ch_layout.nb_channels, 0);
            av_opt_set_int(swr_ctx, "in_sample_rate", codec_ctx->sample_rate, 0);
            av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", codec_ctx->sample_fmt, 0);
            av_opt_set_int(swr_ctx, "out_channel_count", codec_ctx->ch_layout.nb_channels, 0);
            av_opt_set_int(swr_ctx, "out_sample_rate", codec_ctx->sample_rate, 0);
            av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
            swr_init(swr_ctx);

            // Start playing audio
            SDL_PauseAudio(0);

            AVPacket packet;
            AVFrame *frame = av_frame_alloc();
            uint8_t *audio_buf = nullptr;

            playing = true;
            while (keep_playing.load() && av_read_frame(fmt_ctx, &packet) >= 0)
            {
                if (packet.stream_index == audio_stream->index)
                {
                    if (avcodec_send_packet(codec_ctx, &packet) >= 0)
                    {
                        while (avcodec_receive_frame(codec_ctx, frame) >= 0)
                        {
                            int dst_nb_samples = static_cast<int>(av_rescale_rnd(swr_get_delay(swr_ctx, codec_ctx->sample_rate) + frame->nb_samples, codec_ctx->sample_rate, codec_ctx->sample_rate, AV_ROUND_UP));
                            int dst_buf_size = av_samples_get_buffer_size(nullptr, codec_ctx->ch_layout.nb_channels, dst_nb_samples, AV_SAMPLE_FMT_FLT, 1);
                            audio_buf = (uint8_t *)av_malloc(dst_buf_size);
                            swr_convert(swr_ctx, &audio_buf, dst_nb_samples, (const uint8_t **)frame->extended_data, frame->nb_samples);
                            SDL_QueueAudio(1, audio_buf, dst_buf_size);
                            
                            av_free(audio_buf);
                        }
                    }
                }
                av_packet_unref(&packet);
            }

            // Clean up
            av_frame_free(&frame);
            swr_free(&swr_ctx);
            avcodec_free_context(&codec_ctx);
            avformat_close_input(&fmt_ctx);
            SDL_CloseAudio();
            playing = false;
            // SDL_Quit();
        }

        bool is_playing() const
        {
            return playing.load();
        }

        auto presets() const {
            // return the preset keys
            std::vector<std::string> keys;
            std::transform(presets_.begin(), presets_.end(), std::back_inserter(keys),
                           [](const auto &preset) { return preset.first; });
            return keys;
        }

        auto has_error() const {
            return has_error_.load();
        }
        auto last_error() const {
            return last_error_;
        }
    private:

        std::map<std::string, std::string> presets_{
            {"Urbana Play", "https://cdn.instream.audio/:9660/stream"},
            {"Radio 1 Rock", "http://stream.radioreklama.bg:80/radio1rock128"},
            {"BBC Radio 1", "http://bbcmedia.ic.llnwd.net/stream/bbcmedia_radio1_mf_p"},
            {"Jazz FM", "http://media-ice.musicradio.com/JazzFMMP3"},
            {"Classic FM", "http://media-ice.musicradio.com/ClassicFM"},
            {"NPR News", "https://npr-ice.streamguys1.com/live.mp3"},
            {"Chilltrax", "http://s3.viastreaming.net:7600/stream"},
            {"France Inter", "http://direct.franceinter.fr/live/franceinter-midfi.mp3"},
            {"Radio Swiss Jazz", "http://stream.srg-ssr.ch/m/rsj/mp3_128"},
            {"Lounge FM", "http://stream.laut.fm/lounge"},
            {"Radio Paradise", "http://stream.radioparadise.com/mp3-192"},
            {"Ambient Sleeping Pill", "http://radio.ambientSleepingPill.com/asp-sr128.mp3"}
        };

        SDL_AudioDeviceID dev;
        std::atomic<bool> playing{false};
        std::atomic<bool> keep_playing{true};
        std::atomic<float> level{0.0f};
        std::atomic<bool> has_error_{false};
        std::string last_error_;
    };
}