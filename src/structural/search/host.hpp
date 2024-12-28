#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace search {
    using action_t = std::function<void()>;
    using match_result_t = std::pair<std::string,action_t>;
    using match_sink_t = std::function<void(match_result_t)>;
    using provider_t = std::function<void(std::string_view, match_sink_t)>;

    struct host {
        void add_provider(provider_t provider, action_t action) {
            providers.push_back({provider, action});
        }
        void operator()(std::string_view query, match_sink_t sink) {
            for (auto const &[provider, action] : providers) {
                provider(query, [action, sink](match_result_t result) {
                    sink({result.first, [action, result] {
                        action();
                        result.second();
                    }});
                });
            }
        }
        std::vector<std::pair<provider_t, action_t>> providers;
    };
}
