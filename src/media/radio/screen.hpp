#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

#include <imgui.h>
#include <SDL2/SDL.h>
#include "host.hpp"

#include "../../imgcache.hpp"
#include "../../registrar.hpp"
#include "location.hpp"

namespace radio
{
    struct screen
    {
        static constexpr auto dial_height{150};

        screen(host &h) : host_{h}
        {
            presets = host_.presets();
            std::transform(presets.begin(), presets.end(), std::back_inserter(presets_c_strs),
                           [](const auto &preset)
                           { return preset.c_str(); });
        }
        ~screen()
        {
        }

        void render()
        {
            if (ImGui::BeginChild("Dial", {0, dial_height}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar))
            {
                auto const dial_width = ImGui::GetWindowWidth();
                auto draw_list{ImGui::GetWindowDrawList()};
                auto const initial_pos = ImGui::GetWindowPos();
                auto const scroll_y = ImGui::GetScrollY();
                auto const offset_y = initial_pos.y - scroll_y;

                constexpr auto green_on = IM_COL32(0, 255, 0, 255);
                constexpr auto green_off = IM_COL32(0, 160, 0, 100);
                constexpr auto red = IM_COL32(255, 0, 0, 255);
                constexpr auto traslucid_gray = IM_COL32(128, 128, 128, 40);
                constexpr auto traslucid_yellow = IM_COL32(255, 255, 0, 40);

                auto const y_center = dial_height / 2;
                auto const selectable_height = dial_height / 7;
                std::array<float, 4> row_y{
                    y_center - 2 * selectable_height - 10.0f,
                    y_center - selectable_height - 5.0f,
                    y_center + selectable_height - 5.0f,
                    y_center + 2 * selectable_height};

                auto const green{host_.is_playing() ? green_on : green_off};

                // let's do the on/off button
                if (ImGui::Button(host_.is_playing() ? ICON_MD_VOLUME_OFF : ICON_MD_VOLUME_MUTE))
                {
                    if (host_.is_playing())
                    {
                        host_.stop();
                        currently_playing.reset();
                    }
                    else
                    {
                        if (currently_playing.has_value())
                        {
                            host_.play(presets[currently_playing.value()]);
                        }
                    }
                }

                auto const cursor_pos {ImGui::GetCursorPos()};
                int current_row{1};
                auto const x_unit = dial_width / (presets.size() + 2);
                ImGui::PushStyleColor(ImGuiCol_Text, green);
                for (auto ip{0}; ip < presets.size(); ++ip)
                {
                    auto const x = initial_pos.x + ip * x_unit;
                    auto start_x = x + 2;
                    draw_list->AddLine(
                        {start_x + 3, offset_y + y_center - 1 + 2 * (current_row > 1)},
                        {start_x + 3, offset_y + y_center - 4 + 8 * (current_row > 1)}, green);
                    draw_list->AddLine(
                        {start_x + 4, offset_y + y_center - 1 + 2 * (current_row > 1)},
                        {start_x + 4, offset_y + y_center - 4 + 8 * (current_row > 1)}, green);
                    draw_list->AddLine(
                        {start_x + 5, offset_y + y_center - 1 + 2 * (current_row > 1)},
                        {start_x + 5, offset_y + y_center - 4 + 8 * (current_row > 1)}, green);
                    ImGui::SetCursorPosX(x);
                    ImGui::SetCursorPosY(row_y[current_row]);
                    if (ImGui::Selectable(presets[ip].c_str(), ip == currently_playing, 0,
                                          {3 * x_unit, selectable_height}))
                    {
                        if (currently_playing != ip)
                        {
                            host_.play(presets[ip]);
                            currently_playing = ip;
                        }
                        else
                        {
                            host_.stop();
                            currently_playing.reset();
                        }
                    }
                    static constexpr int mapping[] = {3, 2, 0, 1};
                    current_row = mapping[current_row];
                }
                ImGui::PopStyleColor();
                auto target_x = currently_playing.has_value() ? (initial_pos.x + currently_playing.value() * x_unit + 4) : initial_pos.x;
                if (dial_x < target_x)
                {
                    dial_x = std::min(target_x, dial_x + (target_x - dial_x) / 10 + 1);
                }
                else if (dial_x > target_x)
                {
                    dial_x = std::max(target_x, dial_x - (dial_x - target_x) / 10 - 1);
                }

                draw_list->AddLine({0, offset_y + y_center - 1}, {dial_width, offset_y + y_center - 1}, green);
                draw_list->AddLine({0, offset_y + y_center + 1}, {dial_width, offset_y + y_center + 1}, green);

                draw_list->AddTriangleFilled(
                    {dial_x + 1, offset_y + 5},
                    {dial_x - 4, offset_y},
                    {dial_x + 6, offset_y}, red);
                draw_list->AddTriangleFilled(
                    {dial_x + 1, offset_y + dial_height - 5},
                    {dial_x - 4, offset_y + dial_height},
                    {dial_x + 6, offset_y + dial_height}, red);
                draw_list->AddLine({dial_x, offset_y}, {dial_x, offset_y + dial_height}, red);
                draw_list->AddLine({dial_x + 1, offset_y}, {dial_x + 1, offset_y + dial_height}, red);
                draw_list->AddLine({dial_x + 2, offset_y}, {dial_x + 2, offset_y + dial_height}, red);

                if (host_.is_playing())
                {
                    draw_list->AddRectFilledMultiColor(
                        {initial_pos.x, offset_y + 3},
                        {initial_pos.x + dial_width, offset_y + dial_height - 6},
                        traslucid_gray, traslucid_gray, traslucid_yellow, traslucid_yellow);
                }

                auto const &title{host_.stream_name()};
                if (!title.empty())
                {
                    ImGui::SetCursorPos(cursor_pos);
                    ImGui::Indent();
                    unsigned long const start = static_cast<unsigned long>(ImGui::GetTime() * 2.0) % title.size();
                    draw_list->AddRectFilled({cursor_pos.x + 20, cursor_pos.y + 50}, {cursor_pos.x + 110, cursor_pos.y + 62}, IM_COL32(0, 0, 0, 190));
                    ImGui::TextUnformatted(title.data() + start,
                                           title.data() + std::min(start + 10ul, static_cast<unsigned long>(title.size())));
                    ImGui::Unindent();
                }

                ImGui::SetCursorPosX(initial_pos.x);
                ImGui::SetCursorPosY(initial_pos.y + dial_height + 10);

                if (host_.has_error())
                {
                    ImGui::Text("Error: %s", host_.last_error().c_str());
                }

                // check if the user pressed the left arrow
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
                {
                    if (currently_playing.has_value() && currently_playing.value() > 0U)
                    {
                        currently_playing = currently_playing.value() - 1;
                        host_.play(presets[currently_playing.value()]);
                    }
                    else
                    {
                        host_.stop();
                        currently_playing.reset();
                    }
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
                {
                    if (currently_playing.has_value())
                    {
                        if (currently_playing < presets.size() - 1)
                        {
                            currently_playing = currently_playing.value() + 1;
                            host_.play(presets[currently_playing.value()]);
                        }
                    }
                    else
                    {
                        currently_playing = 0;
                        host_.play(presets[currently_playing.value()]);
                    }
                }
            }
            ImGui::EndChild();
            if (host_.is_playing()) {
                auto new_location = location_.render(host_.total_run(), host_.current_run());
                if (new_location.has_value()) {
                    host_.move_to(std::chrono::milliseconds{static_cast<long long>(new_location.value() * host_.total_run().count())});
                }
            }
            else {
                location_.render(std::chrono::milliseconds{0}, std::chrono::milliseconds{0});
            }
        }

    private:
        std::vector<std::string> presets;
        std::vector<const char *> presets_c_strs;
        std::optional<unsigned> currently_playing{};
        host &host_;
        img_cache &img_cache_ = *registrar::get<img_cache>({});
        float dial_x{};
        media::radio::location location_;
    };
}