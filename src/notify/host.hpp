#pragma once

#include <functional>
#include <string>

namespace notify {
    struct host
    {
        using sink_t = std::function<void(std::string_view, std::string_view)>;

        host &operator()(std::string_view text, std::string_view topic = {}) {
            notify_(text, topic);
            return *this;
        }

        void sink(sink_t notify) {
            notify_ = [old = notify_, notify](std::string_view text, std::string_view topic) {
                notify(text, topic);
                old(text, topic);
            };
        }
    private:
        sink_t notify_ = [](std::string_view, std::string_view) {};
    };
}