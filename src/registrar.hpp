#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class registrar {
public:
    // Adds or updates a service of a specific type and key
    template <typename T>
    static void add(const std::string& key, std::shared_ptr<T> service) {
        std::lock_guard<std::mutex> lock(getMutex());
        getTypeMap<T>()[key] = service;
    }

    // Retrieves a service of a specific type and key
    template <typename T>
    static std::shared_ptr<T> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(getMutex());

        auto& typeMap = getTypeMap<T>();
        auto it = typeMap.find(key);
        if (it == typeMap.end()) {
            throw std::runtime_error("Service not found for the given key");
        }
        return it->second;
    }

    // Retrieves all keys for a given type
    template <typename T>
    static void keys(std::function<void(std::string const&)> sink) {
        std::lock_guard<std::mutex> lock(getMutex());

        auto& typeMap = getTypeMap<T>();
        for (const auto& [key, _] : typeMap) {
            sink(key);
        }
    }

    template<typename T>
    static void all(std::function<void(std::string const&, std::shared_ptr<T>)> sink) {
        std::lock_guard<std::mutex> lock(getMutex());

        auto& typeMap = getTypeMap<T>();
        for (const auto& [key, p] : typeMap) {
            sink(key, p);
        }
    }

    // Removes a service from the list
    template <typename T>
    static void remove(std::string const &key) {
        std::lock_guard<std::mutex> lock(getMutex());
        getTypeMap().erase(key);
    }

private:
    // Type-erased storage for different service types
    template <typename T>
    using TypeMap = std::unordered_map<std::string, std::shared_ptr<T>>;

    // Access the type-specific map
    template <typename T>
    static TypeMap<T>& getTypeMap() {
        static TypeMap<T> typeMap;
        return typeMap;
    }

    // Provides a reference to the mutex
    static std::mutex& getMutex() {
        static std::mutex mutex;
        return mutex;
    }
};
