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
            SDL_AudioSpec wanted_spec{}, obtained_spec{};
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
                            int dst_buf_size = av_samples_get_buffer_size(nullptr, obtained_spec.channels, dst_nb_samples, AV_SAMPLE_FMT_FLT, 1);
                            audio_buf = (uint8_t *)av_malloc(dst_buf_size);
                            auto converted = swr_convert(swr_ctx, &audio_buf, dst_nb_samples, (const uint8_t **)frame->extended_data, frame->nb_samples);
                            if (converted < 0)
                            {
                                av_free(audio_buf);
                                throw std::runtime_error("Failed to convert audio");
                            }
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
            {"AM 750 AR", "https://26513.live.streamtheworld.com/AM750_SC"},
            {"BBC Radio 3", "http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_radio_three/bbc_radio_three.isml/bbc_radio_three-audio%3d96000.norewind.m3u8"},
            {"BBC Radio 4", "http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_radio_fourfm/bbc_radio_fourfm.isml/bbc_radio_fourfm-audio%3d96000.norewind.m3u8"},
            {"BBC World Service", "http://a.files.bbci.co.uk/media/live/manifesto/audio/simulcast/hls/nonuk/sbr_low/ak/bbc_world_service.m3u8"},
            {"Cadena Ser", "https://25653.live.streamtheworld.com:443/CADENASERAAC_SC"},
            {"Classic FM", "http://media-ice.musicradio.com/ClassicFM"},
            {"France Inter", "http://direct.franceinter.fr/live/franceinter-midfi.mp3"},
            {"La Red (AR)", "https://27373.live.streamtheworld.com:443/LA_RED_AM910AAC_SC"},
            {"Lounge FM", "http://stream.laut.fm/lounge"},
            {"NPR News", "https://npr-ice.streamguys1.com/live.mp3"},
            {"Qmusic", "http://playerservices.streamtheworld.com/api/livestream-redirect/QMUSIC.mp3"},
            {"Radio 1 Rock", "http://stream.radioreklama.bg:80/radio1rock128"},
            {"Radio 10", "http://playerservices.streamtheworld.com/api/livestream-redirect/RADIO10.mp3"},
            {"Radio 4", "http://playerservices.streamtheworld.com/api/livestream-redirect/RADIO4.mp3"},
            {"Radio 538", "http://playerservices.streamtheworld.com/api/livestream-redirect/RADIO538.mp3"},
            {"Monte Carlo 2", "https://n23a-eu.rcs.revma.com/fxp289cp81uvv"},
            {"Radio Nova", "http://novazz.ice.infomaniak.ch/novazz-128.mp3"},
            {"Radio Paradise", "http://stream.radioparadise.com/mp3-192"},
            {"Radio SRF 3", "http://stream.srg-ssr.ch/m/drs3/mp3_128"},
            {"Swiss Jazz", "http://stream.srg-ssr.ch/m/rsj/mp3_128"},
            {"Sky Radio", "http://playerservices.streamtheworld.com/api/livestream-redirect/SKYRADIO.mp3"},
            {"TWiT (This Week in Tech)", "https://pdst.fm/e/pscrb.fm/rss/p/cdn.twit.tv/libsyn/twit_1004/b26992aa-b1b1-4f90-a2b7-efc6a7d4b42a/R1_twit1004.mp3"},
            {"Urbana Play", "https://cdn.instream.audio/:9660/stream"},
            {"RMC", "https://icy.unitedradio.it/RMC.aac"},
        };

        SDL_AudioDeviceID dev;
        std::atomic<bool> playing{false};
        std::atomic<bool> keep_playing{true};
        std::atomic<float> level{0.0f};
        std::atomic<bool> has_error_{false};
        std::string last_error_;
    };
}