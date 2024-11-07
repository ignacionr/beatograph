#pragma once

#include <imgui.h>
#include "../host/host.hpp"
#include "../host/screen.hpp"
#include "../views/assertion.hpp"

namespace dev_locked
{
    struct screen
    {
        screen(host_local &localhost) : localhost{localhost} {}
        void render()
        {
            hs.render(host::by_name("dev-locked"), localhost);
            if (ImGui::CollapsingHeader("Status"))
            {
                views::Assertion("dev1_container is running", [this]
                                 { return host::by_name("dev-locked")->docker().is_container_running("dev1_container", localhost); });
                ImGui::Indent();
                views::Assertion("xrdp is running for dev1", [this]
                                 { return host::by_name("dev-locked")->docker().is_process_running("dev1_container", "xrdp", localhost); });
                views::Assertion("dev1 can log in for gitservice usage with ssh", [this]
                                 {
                        auto const result = host::by_name("dev-locked")->docker().execute_command("su - dev1 -c 'ssh -T git@gitservice'", "dev1_container", localhost, false);
                        return result.find("Welcome to git-server-docker!") != std::string::npos; });
                ImGui::Unindent();
                views::Assertion("dev2_container is running", [this]
                                 { return host::by_name("dev-locked")->docker().is_container_running("dev2_container", localhost); });
                ImGui::Indent();
                views::Assertion("xrdp is running for dev2", [this]
                                 { return host::by_name("dev-locked")->docker().is_process_running("dev2_container", "xrdp", localhost); });
                views::Assertion("dev2 can log in for gitservice usage with ssh", [this]
                                 {
                        auto const result = host::by_name("dev-locked")->docker().execute_command("su - dev2 -c 'ssh -T git@gitservice'", "dev2_container", localhost, false);
                        return result.find("Welcome to git-server-docker!") != std::string::npos; });
                ImGui::Unindent();
                views::Assertion("dev3_container is running", [this]
                                 { return host::by_name("dev-locked")->docker().is_container_running("dev3_container", localhost); });
                ImGui::Indent();
                views::Assertion("xrdp is running for dev3", [this]
                                 { return host::by_name("dev-locked")->docker().is_process_running("dev3_container", "xrdp", localhost); });
                views::Assertion("dev3 can log in for gitservice usage with ssh", [this]
                                 {
                        auto const result = host::by_name("dev-locked")->docker().execute_command("su - dev3 -c 'ssh -T git@gitservice'", "dev3_container", localhost, false);
                        return result.find("Welcome to git-server-docker!") != std::string::npos; });
                ImGui::Unindent();
                views::Assertion("The Git service container is running", [this]
                                 { return host::by_name("dev-locked")->docker().is_container_running("gitservice", localhost); });
                ImGui::Indent();
                ImGui::Unindent();
                views::Assertion("The externalizer service is running", [this]
                                 { return host::by_name("dev-locked")->docker().is_container_running("socat-proxy", localhost); });
                ImGui::Indent();
                ImGui::Unindent();
                views::Assertion("The pipeline runner service is running", [this]
                                 { return host::by_name("dev-locked")->docker().is_container_running("pipeline-runner", localhost); });
                ImGui::Indent();
                views::Assertion("root on the pipeline runner container can log in for gitservice usage with ssh", [this]
                                 {
                        auto const result = host::by_name("dev-locked")->docker().execute_command("ssh -T git@gitservice", "pipeline-runner", localhost, false);
                        return result.find("Welcome to git-server-docker!") != std::string::npos; });
                ImGui::Unindent();
            }
            if (ImGui::CollapsingHeader("Corrective Actions"))
            {
                static std::string seed_result;
                if (ImGui::Button("Seed Containers"))
                {
                    seed_result = host::by_name("dev-locked")->docker().execute_command("bash -c 'cd /home/ubuntu/arangodb-infra/dev-locked && ./seed.sh'", localhost, false);
                }
                if (!seed_result.empty())
                {
                    ImGui::Text("%s", seed_result.c_str());
                }
            }
        }
    private:
        host_screen hs;
        host_local& localhost;
    };
};