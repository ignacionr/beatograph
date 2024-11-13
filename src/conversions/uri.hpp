#pragma once

#include <string>

namespace conversions
{
    struct uri {
        static std::string encode_component(std::string src) {
            std::string dst;
            for (auto c : src) {
                if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    dst += c;
                }
                else {
                    dst += '%';
                    dst += "0123456789ABCDEF"[static_cast<unsigned char>(c) >> 4];
                    dst += "0123456789ABCDEF"[static_cast<unsigned char>(c) & 0xf];
                }
            }
            return dst;
        }
    };
} // namespace conversions
