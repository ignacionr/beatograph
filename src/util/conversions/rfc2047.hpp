#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cctype>
#include <map>
#include "base64.hpp"

namespace conversions
{

    // Quoted-Printable decoding function
    std::string quoted_printable_decode(std::string_view input)
    {
        std::ostringstream decoded;
        size_t i = 0;

        while (i < input.length())
        {
            if (input[i] == '=' && i + 2 < input.length())
            {
                char hex[3] = {input[i + 1], input[i + 2], '\0'};
                decoded << static_cast<char>(std::strtol(hex, nullptr, 16));
                i += 3;
            }
            else if (input[i] == '_')
            {
                decoded << ' ';
                i++;
            }
            else
            {
                decoded << input[i++];
            }
        }
        return decoded.str();
    }

    // Function to decode an RFC 2047 encoded-word
    std::string decode_rfc2047(std::string_view input)
    {
        std::string result;
        size_t pos = 0;

        while (pos < input.length())
        {
            size_t start = input.find("=?", pos);
            if (start == std::string::npos)
            {
                result += input.substr(pos);
                break;
            }

            result += input.substr(pos, start - pos); // Append normal text
            size_t end = input.find("?=", start);
            if (end == std::string::npos)
                break;

            auto encoded_word = input.substr(start, end - start + 2);
            size_t q1 = encoded_word.find("?", 2);
            size_t q2 = encoded_word.find("?", q1 + 1);
            size_t q3 = encoded_word.find("?", q2 + 1);

            if (q1 == std::string::npos || q2 == std::string::npos || q3 == std::string::npos)
                break;

            auto charset = encoded_word.substr(2, q1 - 2);
            auto encoding = encoded_word.substr(q1 + 1, q2 - q1 - 1);
            auto data = encoded_word.substr(q2 + 1, q3 - q2 - 1);

            std::string decoded_text;
            if (encoding == "B" || encoding == "b")
            {
                decoded_text = base64_to_text(std::string{data});
            }
            else if (encoding == "Q" || encoding == "q")
            {
                decoded_text = quoted_printable_decode(data);
            }

            // For simplicity, assume UTF-8 (libcurl doesn’t provide charset conversion)
            result += decoded_text;
            pos = end + 2;
        }

        return result;
    }
}
