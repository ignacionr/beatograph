#pragma once

struct calendar_screen {
    void render() {
        if (ImGui::BeginChild("Calendar")) {
            static const char* days[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
            ImGui::Columns(7);
            for (int i = 0; i < 7; ++i) {
                ImGui::Text("%s", days[i]);
                ImGui::NextColumn();
            }
            ImGui::EndChild();
        }
    }
};