#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <imgui.h>

namespace views {
    template<typename cached_t>
    void cached_view(std::string const &name, std::function<cached_t()> const &factory, 
    std::function<void(cached_t const &)> const &renderer) 
    {
        static std::unordered_map<int, std::optional<cached_t>> cache;
        if (ImGui::CollapsingHeader(name.c_str())) {
            auto &cached{cache[ImGui::GetID(name.c_str())]};
            if (!cached.has_value()) {
                cached = factory();
            }
            renderer(cached.value());
            if (ImGui::Button("Refresh")) {
                cached.reset();
            }
        }
    }
}