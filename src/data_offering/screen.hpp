#pragma once

#include <imgui.h>
#include "importer_report.hpp"

struct dataoffering_screen {
    void render() {
        ImGui::BeginChild("Data Offering");
        ImGui::Text("* Is OddsMetrix working?");
        ImGui::Text("* Is our importer working?");
        importer.render();
        ImGui::Text("* RabbitMQ Queue");
        ImGui::Text("* Storm Importer Status");
        ImGui::Text("* Hadoop Status");
        ImGui::Text("* Hbase Status");
        ImGui::EndChild();
    }
private:
    importer_report importer;
};
