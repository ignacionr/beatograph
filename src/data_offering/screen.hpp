#pragma once

#include <imgui.h>
#include "importer_report.hpp"
#include "../host/host.hpp"
#include "../host/screen.hpp"

struct dataoffering_screen {
    dataoffering_screen(host_local &localhost) : importer{localhost}, localhost_{localhost} {}
    void render() {
        ImGui::BeginChild("Data Offering");
        ImGui::Text("* Is OddsMetrix working?");
        ImGui::Text("* Is our importer working?");
        importer.render();
        ImGui::Text("* RabbitMQ Queue - runs in dockercontainer in storm1");
        host_screen_.render(host::by_name("storm1"), localhost_);
        ImGui::Text("* Storm Importer Status");
        ImGui::Text("* Hadoop Status");
        ImGui::Text("* Hbase Status");
        ImGui::EndChild();
    }
private:
    importer_report importer;
    host_local &localhost_;
    host_screen host_screen_;
};
