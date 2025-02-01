#pragma once

#include "pch.h"

#include <string>
#include <unordered_map>

using namespace std::string_literals;

namespace hosting::telnet {
    struct option {
        option(char code) : code_{code} {
        }

        std::string DO() const {
            return "\xFF\xFD"s + code_;
        }

        std::string DONT() const {
            return "\xFF\xFE"s + code_;
        }

        std::string WILL() const {
            return "\xFF\xFB"s + code_;
        }

        std::string WONT() const {
            return "\xFF\xFC"s + code_;
        }

        std::string reply_DO() {
            if (do_replied_) {
                return {};
            }
            do_replied_ = true;
            if (allowed_) {
                enabled_ = true;
                return WILL();
            }
            else {
                return WONT();
            }
        }

        std::string reply_DONT() {
            if (do_replied_) {
                return {};
            }
            do_replied_ = true;
            if (forced_) {
                return WILL();
            }
            else {
                enabled_ = false;
                return WONT();
            }
        }

        std::string reply_WILL() {
            if (will_replied_) {
                return {};
            }
            will_replied_ = true;
            if (allowed_) {
                enabled_ = true;
                return DO();
            }
            else {
                return DONT();
            }
        }

        std::string reply_WONT() {
            if (will_replied_) {
                return {};
            }
            will_replied_ = true;
            if (forced_) {
                return DO();
            }
            else {
                enabled_ = false;
                return DONT();
            }
        }

        char code_;
        bool enabled_{false};
        bool allowed_{true};
        bool forced_{false};
        bool will_replied_{false};
        bool do_replied_{false};
    };
    struct option_binary : option {
        option_binary() : option{code()} {
            enabled_ = true;
        }
        static char code() {return '\x00';}
    };
    struct option_echo : option {
        option_echo() : option{code()} {
            enabled_ = true;
        }
        static char code() {return '\x01';}
    };
    struct option_unknown : option {
        option_unknown(char code) : option{code} {
            allowed_ = false;
        }
    };

    struct option_group {
        void add(option&& opt) {
            options.emplace(opt.code_, std::move(opt));
        }
        option& operator[](char code) {
            if (auto it = options.find(code); it != options.end()) {
                return it->second;
            }
            return options.emplace(code, option_unknown{code}).first->second;
        }
        std::string reply(const char* buffer) {
            auto &option = (*this)[buffer[2]];
            switch (buffer[1]) {
            case '\xFB':
                return option.reply_DO();
            case '\xFC':
                return option.reply_DONT();
            case '\xFD':
                return option.reply_WILL();
            case '\xFE':
                return option.reply_WONT();
            default:
                return "";
            }
        }
        std::string initiate() const {
            std::string result;
            for (auto const &[_, option] : options) {
                if (option.forced_) {
                    result += option.DO();
                }
                else if (option.enabled_) {
                    result += option.WILL();
                }
            }
            return result;
        }

        std::unordered_map<char, option> options;
    };
}