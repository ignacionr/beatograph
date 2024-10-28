#include <functional>
#include <optional>
#include <string>
#include <expected>
#include <unordered_map>
#include <imgui.h>

namespace views {
    template<typename cached_t>
    void cached_view(std::string const &name, std::function<cached_t()> const &factory, 
    std::function<void(cached_t const &)> const &renderer) 
    {
        static std::unordered_map<int, std::optional<std::expected<cached_t, std::string>>> cache;
        if (ImGui::CollapsingHeader(name.c_str())) {
            auto &cached{cache[ImGui::GetID(name.c_str())]};
            if (!cached.has_value()) {
                try {
                    cached = factory();
                }
                catch(std::exception const &ex) {
                    cached = std::unexpected(ex.what());
                }
            }
            auto &cached_value{cached.value()};
            if (cached_value.has_value()) {
                renderer(cached_value.value());
            }
            else {
                ImGui::Text("Error: %s", cached_value.error().c_str());
            }
            if (ImGui::Button("Refresh")) {
                cached.reset();
            }
        }
    }
}