#pragma once

#include <functional>
#include <string>

namespace conversions
{
    struct string_double
    {
        static double to_double(std::string src)
        {
            return std::stod(src);
        }
        static std::string to_string(double src)
        {
            return std::to_string(src);
        }
        static std::function<std::string(std::string)> convert(std::function<double(double)> conv)
        {
            return [conv](std::string src) {
                return to_string(conv(to_double(src)));
            };
        }
    };
} // namespace conversions
