#pragma once

#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace pm::overall
{
    struct solution
    {
        std::string const name;
        std::string const description;
    };

    struct matter
    {
        std::string const name;
        std::string const description;
        std::vector<solution> solutions;
        std::vector<std::pair<solution *, std::string>> configured_solutions;
    };

    struct devops_angle
    {
        std::array<matter, 7> matters = {{{"Infrastructure as Code (IaC)", "How will we define and manage our infrastructure using code? What tools (e.g., Terraform, Ansible, CloudFormation) will we use for IaC?"},
                                          {"Continuous Integration/Continuous Deployment (CI/CD)", "What CI/CD pipeline will we implement? Which tools (e.g., Jenkins, GitLab CI, GitHub Actions) will we use for CI/CD?"},
                                          {"Monitoring and Logging", "How will we monitor our applications and infrastructure? What logging and monitoring tools (e.g., Prometheus, Grafana, ELK Stack) will we use?"},
                                          {"Security and Compliance", "How will we ensure the security of our infrastructure and applications? What measures will we take to comply with relevant regulations and standards?"},
                                          {"Configuration Management", "How will we manage configuration across different environments? What tools (e.g., Chef, Puppet, SaltStack) will we use for configuration management?"},
                                          {"Backup and Disaster Recovery", "What is our strategy for data backup and disaster recovery? How will we ensure business continuity in case of a failure?"},
                                          {"Collaboration and Communication", "How will we facilitate collaboration and communication among team members? What tools (e.g., Slack, Microsoft Teams, Jira) will we use for project management and communication?"}}};
    };
}