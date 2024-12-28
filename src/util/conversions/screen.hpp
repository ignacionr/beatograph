#pragma once

#include <array>
#include <functional>
#include <map>
#include <string>
#include <imgui.h>

#include "base64.hpp"
#include "string-double.hpp"
#include "metric-imperial.hpp"
#include "uri.hpp"

namespace conversions
{
    struct screen
    {
        void render()
        {
            for (const auto &[name, conversion] : get_conversions())
            {
                auto &inout = states[name];
                if (inout[0].reserve(1024); ImGui::InputText(name.c_str(), inout[0].data(), inout[0].capacity()))
                {
                    inout[0] = inout[0].data();
                    if (inout[0].empty())
                    {
                        inout[1].clear();
                    }
                    else
                    {
                        try
                        {
                            inout[1] = conversion(inout[0]);
                        }
                        catch (std::exception &e)
                        {
                            inout[1] = e.what();
                        }
                    }
                }
                if (!inout[1].empty()) {
                    ImGui::TextUnformatted(inout[1].data(), inout[1].data() + inout[1].size());
                    if (ImGui::SmallButton("Copy")) {
                        ImGui::SetClipboardText(inout[1].c_str());
                    }
                }
            }
        }
        using map_t = std::map<std::string, std::function<std::string(std::string)>>;
        static const map_t &get_conversions()
        {
            static const map_t conversions{
                {"Text to Base64", text_to_base64},
                {"Base64 to Text", base64_to_text},
                {"Meters to Feet", string_double::convert(metric_imperial::meters_to_feet)},
                {"Feet to Meters", string_double::convert(metric_imperial::feet_to_meters)},
                {"Text to URI Component", uri::encode_component}
                };
            return conversions;
        }

        std::map<std::string, std::array<std::string, 2>> states;
    };
}