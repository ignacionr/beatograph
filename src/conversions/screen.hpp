#pragma once

#include <array>
#include <functional>
#include <map>
#include <string>
#include <imgui.h>

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
            static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            static const map_t conversions{
                {"Text to Base64", [](std::string src)
                 {
                     std::string ret;
                     int i = 0;
                     int j = 0;
                     unsigned char char_array_3[3];
                     unsigned char char_array_4[4];
                     for (const auto &c : src)
                     {
                         char_array_3[i++] = c;
                         if (i == 3)
                         {
                             char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                             char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                             char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                             char_array_4[3] = char_array_3[2] & 0x3f;
                             for (i = 0; (i < 4); i++)
                             {
                                 ret += base64_chars[char_array_4[i]];
                             }
                             i = 0;
                         }
                     }
                     if (i)
                     {
                         for (j = i; j < 3; j++)
                         {
                             char_array_3[j] = '\0';
                         }
                         char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                         char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                         char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                         char_array_4[3] = char_array_3[2] & 0x3f;
                         for (j = 0; (j < i + 1); j++)
                         {
                             ret += base64_chars[char_array_4[j]];
                         }
                         while ((i++ < 3))
                         {
                             ret += '=';
                         }
                     }
                     return ret;
                 }},
            { "Base64 to Text", [](std::string src) {
                std::string ret;
                int i = 0;
                int j = 0;
                unsigned char char_array_4[4];
                unsigned char char_array_3[3];
                for (const auto &c : src)
                {
                    if (c == '=') break;
                    if (c == '\n' || c == '\r') continue;
                    char_array_4[i++] = c;
                    if (i == 4)
                    {
                        for (i = 0; i < 4; i++)
                        {
                            char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));
                        }
                        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
                        for (i = 0; (i < 3); i++)
                        {
                            ret += char_array_3[i];
                        }
                        i = 0;
                    }
                }
                if (i)
                {
                    for (j = i; j < 4; j++)
                    {
                        char_array_4[j] = 0;
                    }
                    for (j = 0; j < 4; j++)
                    {
                        char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));
                    }
                    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
                    for (j = 0; (j < i - 1); j++)
                    {
                        ret += char_array_3[j];
                    }
                }
                return ret;
            }}};
            return conversions;
        }
    
        std::map<std::string, std::array<std::string,2>> states;
    };
}