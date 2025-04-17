#pragma once

#include "../external/IconsMaterialDesign.h"
#include <imgui/imgui.h>

void setup_fonts()
{
    auto &io{ImGui::GetIO()};
    float baseFontSize = 13.5f;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
    builder.AddChar(static_cast<ImWchar>(0x1F4C5));
    builder.AddChar(static_cast<ImWchar>(0x2019));
    builder.AddChar(static_cast<ImWchar>(0x2603));

    static ImVector<ImWchar> glyphRanges;
    builder.BuildRanges(&glyphRanges);
    io.Fonts->AddFontFromFileTTF("assets/fonts/NotoSansMono-Regular.ttf", baseFontSize, nullptr, glyphRanges.Data);

    float iconFontSize = baseFontSize * 3.0f / 3.0f;

    static const ImWchar icons_ranges[] = {
        static_cast<ImWchar>(ICON_MIN_MD),
        static_cast<ImWchar>(ICON_MAX_MD), 0};
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.GlyphOffset.y = 3;
    io.Fonts->AddFontFromFileTTF("assets/fonts/MaterialIcons-Regular.ttf", iconFontSize, &icons_config, icons_ranges);

    io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-Regular.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-Bold.ttf", 23.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-Italic.ttf", iconFontSize, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-Bold.ttf", iconFontSize, nullptr, io.Fonts->GetGlyphRangesCyrillic());
}