#pragma once

#include <imgui.h>

struct dataoffering_screen {
    void render() {
        ImGui::BeginChild("Data Offering");
        ImGui::Text("* Is OddsMetrix working?");
        ImGui::Text("* Is our importer working?");
        ImGui::Text("* RabbitMQ Queue");
        ImGui::Text("* Storm Importer Status");
        ImGui::Text("* Hadoop Status");
        ImGui::Text("* Hbase Status");
        ImGui::EndChild();
    }
};
