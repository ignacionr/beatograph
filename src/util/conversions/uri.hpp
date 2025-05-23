#pragma once

#include <string>

namespace conversions
{
    struct uri {
        static std::string encode_component(std::string_view src) {
            std::string dst;
            for (unsigned char c : src) {
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
