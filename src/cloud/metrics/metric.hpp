#pragma once
#include <string>

struct metric {
    std::string name;
    std::string help;
    std::string type;

    bool operator==(const metric& other) const {
        return name == other.name;
    }

    bool operator!=(const metric& other) const {
        return !(*this == other);
    }

    bool operator<(const metric& other) const {
        return name < other.name;
    }
};

namespace std {
    template<>
    struct hash<metric> {
        std::size_t operator()(const metric& m) const {
            return std::hash<std::string>{}(m.name);
        }
    };
}
