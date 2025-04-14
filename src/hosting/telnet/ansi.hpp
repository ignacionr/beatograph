#pragma once

#include <string>

namespace hosting::telnet {
    struct ansi {
        static std::string_view reset() { return "\033[0m"; }
        static std::string_view bold() { return "\033[1m"; }
        static std::string_view underline() { return "\033[4m"; }
        static std::string_view blink() { return "\033[5m"; }
        static std::string_view reverse() { return "\033[7m"; }

        static std::string_view black() { return "\033[30m"; }
        static std::string_view red() { return "\033[31m"; }
        static std::string_view green() { return "\033[32m"; }
        static std::string_view yellow() { return "\033[33m"; }
        static std::string_view blue() { return "\033[34m"; }
        static std::string_view magenta() { return "\033[35m"; }
        static std::string_view cyan() { return "\033[36m"; }
        static std::string_view white() { return "\033[37m"; }

        static std::string_view bg_black() { return "\033[40m"; }
        static std::string_view bg_red() { return "\033[41m"; }
        static std::string_view bg_green() { return "\033[42m"; }
        static std::string_view bg_yellow() { return "\033[43m"; }
        static std::string_view bg_blue() { return "\033[44m"; }
        static std::string_view bg_magenta() { return "\033[45m"; }
        static std::string_view bg_cyan() { return "\033[46m"; }
        static std::string_view bg_white() { return "\033[47m"; }

        static std::string_view clear_screen() { return "\033[2J"; }
        static std::string_view clear_line() { return "\033[2K"; }
        static std::string_view home() { return "\033[H"; }

        static std::string_view cursor_up() { return "\033[A"; }
        static std::string_view cursor_down() { return "\033[B"; }
        static std::string_view cursor_forward() { return "\033[C"; }
        static std::string_view cursor_back() { return "\033[D"; }

        static std::string cursor_position(int x, int y) {
            return "\033[" + std::to_string(y) + ";" + std::to_string(x) + "H";
        }
    };
}