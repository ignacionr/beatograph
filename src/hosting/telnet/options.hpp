#pragma once

#include "pch.h"

#include <string>
#include <unordered_map>
#include <memory>

#include "ansi.hpp"

using namespace std::string_literals;

namespace hosting::telnet {
    struct option {
        option(char code) : code_{code} {
        }
        virtual ~option() = default;

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

        virtual std::string reply_DO() {
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

        virtual std::string reply_SUB(std::string_view) {
            return {};
        }

        virtual std::string reply_DONT() {
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

        virtual std::string reply_WILL() {
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

        virtual std::string reply_WONT() {
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

    struct option_terminal_type : option {
        option_terminal_type() : option{code()} {
            enabled_ = true;
            forced_ = true;
        }
        static char code() { return '\x18'; }

        std::string reply_DO() override {
            return "\xFF\xFA"s + code_ + "\x01\xFF\xF0"s;
        }

        std::string reply_SUB(std::string_view sub_command) override {
            if (sub_command[0] == 0) {
                terminal_type_ = sub_command.substr(1);
                return {};
            }
            throw std::runtime_error("unknown sub-command");
        }

        std::string terminal_type_;
    };

    struct option_environment : option {
        option_environment() : option{code()} {
            enabled_ = true;
            forced_ = true;
        }
        static char code() { return '\x24'; }
    };

    struct option_unknown : option {
        option_unknown(char code) : option{code} {
            allowed_ = false;
        }
    };

    struct option_group {
        void add(std::unique_ptr<option>&& opt) {
            options.emplace(opt->code_, std::move(opt));
        }
        std::unique_ptr<option>& operator[](char code) {
            if (auto it = options.find(code); it != options.end()) {
                return it->second;
            }
            return options.emplace(code, std::make_unique<option_unknown>(code)).first->second;
        }
        std::string reply(const char* buffer, size_t &skip) {
            auto &option = (*this)[buffer[2]];
            skip = 3;
            switch (buffer[1]) {
            case '\xFA':
            {
                // this is a sub-command, they end is marked by \xFF\xF0
                auto end = std::find(buffer + 3, buffer + 1024, '\xFF');
                if (end == buffer + 1024) {
                    throw std::runtime_error("sub-command too long");
                }
                if (end + 1 == buffer + 1024 || end[1] != '\xF0') {
                    throw std::runtime_error("sub-command not terminated");
                }
                skip = end - buffer + 2;
                std::string_view sub_command{buffer + 3, static_cast<size_t>(end - buffer - 3)};
                return option->reply_SUB(sub_command);
            }
            case '\xFB':
                return option->reply_DO();
            case '\xFC':
                return option->reply_DONT();
            case '\xFD':
                return option->reply_WILL();
            case '\xFE':
                return option->reply_WONT();
            default:
                return {};
            }
        }
        std::string initiate() const {
            std::string result;
            for (auto const &[_, option] : options) {
                if (option->forced_) {
                    result += option->DO();
                }
                else if (option->enabled_) {
                    result += option->WILL();
                }
            }
            return result;
        }

        std::unordered_map<char, std::unique_ptr<option>> options;
    };
}