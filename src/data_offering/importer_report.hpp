#pragma once

#include <imgui.h>

struct importer_report {
    void render() {
        ImGui::BeginChild("Importer Report");
        ImGui::Text("* Is OddsMetrix working?");
        ImGui::Text("* Is our importer working?");
        ImGui::Text("* RabbitMQ Queue");
        ImGui::Text("* Storm Importer Status");
        ImGui::Text("* Hadoop Status");
        ImGui::Text("* Hbase Status");
        ImGui::EndChild();
    }
};