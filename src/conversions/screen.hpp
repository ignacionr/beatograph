#pragma once

#include <array>
#include <functional>
#include <map>
#include <string>
#include <imgui.h>

#include "base64.hpp"

namespace conversions
{
    struct screen
    {
        void render()
        {
            for (const auto &[name, conversion] : get_conversions())
            {
                auto &inout = states[name];
                if (inout[0].reserve(1024); ImGui::InputText(name.c_str(), inout[0].data(), inout[0].capacity())) {
                    inout[0].resize(std::strlen(inout[0].data()));
                    inout[1] = conversion(inout[0]);
                }
                ImGui::Text("Result: %s", inout[1].c_str());
            }
        }
        using map_t = std::map<std::string, std::function<std::string(std::string)>>;
        static const map_t &get_conversions()
        {
            static const map_t conversions {
                {"Text to Base64", text_to_base64},
            { "Base64 to Text", base64_to_text}};
            return conversions;
        }
    
        std::map<std::string, std::array<std::string,2>> states;
    };
}