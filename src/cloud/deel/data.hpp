#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace cloud::deel::data
{
    struct contract_t {
        std::string id;
        std::string name;
        std::string contractType;
        std::string timezone;
        std::string country;
        bool canClientSubmitPaygWorkReports;
        bool canContractorSubmitPaygWorkReports;
    };

    struct paymentCycle_t {
        std::chrono::system_clock::time_point start;
        std::chrono::system_clock::time_point end;
        std::chrono::system_clock::time_point due;
    };

    struct client_t {
        std::string name;
        std::string displayName;
        std::string picUrl;
    };

    struct organization_t {
        std::string name;
    };

    struct history_row {
        long id;
        std::string label;
        std::string invoiceId;
        std::string description;
        std::string status;
        std::string currency;
        std::string paymentCurrency;
        std::string paymentMethod;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point paidAt;
        std::chrono::system_clock::time_point moneyReceivedAt;
        std::string type;
        std::chrono::system_clock::time_point issuedAt;
        std::string amount;
        std::string total;
        contract_t contract;
        paymentCycle_t paymentCycle;
        client_t client;
        organization_t organization;
    };

    struct history {
        std::vector<history_row> rows;
    };

} // namespace cloud::deel
