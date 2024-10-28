#pragma once

#include <iostream>
#include <map>
#include <vector>
#include <functional>

template <typename T>
class Repository {
public:
    // Add an observer to the repository
    static void when_available(std::function<void(T)> observer) {
        if (getResolver().has_value()) {
            observer(getResolver().value());
        }
        else getObservers().emplace_back(observer);
    }

    // Notify observers when a subsystem becomes available
    static void available(T value) {
        for (auto& observer : getObservers()) {
            observer(value);
        }
        getResolver() = value;
    }

private:
    static std::vector<std::function<void(T)>>& getObservers() {
        static std::vector<std::function<void(T)>> observers_;
        return observers_;
    }

    static std::optional<T>& getResolver() {
        static std::optional<T> resolver_;
        return resolver_;
    }
};
