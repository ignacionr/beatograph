#pragma once

#include <functional>
#include <optional>
#include <string>

template<typename T>
concept KeyValue = requires(T i) {
    { i.set(std::string{}, std::optional<std::string>{}) };
    { i.get(std::string{}) } -> std::same_as<std::optional<std::string>>;
    { i.scan_level(std::string_view{}, std::declval<std::function<void(std::string_view)>>()) };
};
