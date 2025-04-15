#pragma once

#include <expected>
#include <format>
#include <functional>
#include <string>

#include "data.hpp"
#include <nlohmann/json.hpp>

namespace cloud::deel {
    using history_res = std::expected<data::history, std::string>;
    template<typename fetch_t>
    struct host {
        host(std::function<std::string()> token_provider) noexcept
            : token_provider_{std::move(token_provider)} {
        }

        static void extract_string(auto const &container, std::string &value, std::string_view field) noexcept {
            if (container.contains(field)) {
                auto const &val = container.at(field);
                if (val.is_string()) {
                    value = val.template get<std::string>();
                }
                else if (val.is_null()) {
                    value = {};
                }
                else {
                    value = std::format("Invalid type for {}: expected string or null", field);
                }
            }
            else {
                value.clear();
            }
        }
        
        [[nodiscard]] history_res fetch_history() const noexcept {
            history_res history;
            try {
                auto json_text = fetch_t{}("https://api.deel.com/invoices/history", 
                    [this](auto setter){
                        setter("accept: application/json, text/plain, */*");
                        setter("x-api-version: 2");
                        setter(std::format("x-auth-token: {}", token_provider_()));
                    });
                auto json = nlohmann::json::parse(json_text);
                for (auto const &row : json.at("rows")) {
                    data::history_row history_row;
                    history_row.id = row.at("id").template get<long>();
                    extract_string(row, history_row.label, "label");
                    history_row.invoiceId = row.at("invoiceId").template get<std::string>();
                    extract_string(row, history_row.description, "description");
                    extract_string(row, history_row.status, "status");
                    extract_string(row, history_row.currency, "currency");
                    extract_string(row, history_row.paymentCurrency, "paymentCurrency");
                    extract_string(row, history_row.paymentMethod, "paymentMethod");
                    extract_string(row, history_row.createdAt, "createdAt");
                    extract_string(row, history_row.paidAt, "paidAt");
                    extract_string(row, history_row.moneyReceivedAt, "moneyReceivedAt");
                    extract_string(row, history_row.type, "type");
                    extract_string(row, history_row.issuedAt, "issuedAt");
                    extract_string(row, history_row.amount, "amount");
                    extract_string(row, history_row.total, "total");
                    auto contract = row.at("contract");
                    if (contract.is_object()) {
                        extract_string(contract, history_row.contract.id, "id");
                        extract_string(contract, history_row.contract.name, "name");
                        extract_string(contract, history_row.contract.contractType, "contractType");
                        extract_string(contract, history_row.contract.timezone, "timezone");
                        extract_string(contract, history_row.contract.country, "country");
                        history_row.contract.canClientSubmitPaygWorkReports =
                            contract.template value<bool>("canClientSubmitPaygWorkReports", false);
                        history_row.contract.canContractorSubmitPaygWorkReports =
                            contract.template value<bool>("canContractorSubmitPaygWorkReports", false);
                    }
                    auto paymentCycle = row.at("paymentCycle");
                    if (paymentCycle.is_object()) {
                        history_row.paymentCycle.start = paymentCycle.value("start", "");
                        history_row.paymentCycle.end = paymentCycle.value("end", "");
                        history_row.paymentCycle.due = paymentCycle.value("due", "");
                    }
                    auto client = row.at("client");
                    if (client.is_object()) {
                        history_row.client.name = client.value("name", "");
                        history_row.client.displayName = client.value("displayName", "");
                        extract_string(client, history_row.client.picUrl, "picUrl");
                    }
                    auto organization = row.at("organization");
                    if (organization.is_object()) {
                        history_row.organization.name = organization.value("name", "");
                    }
                    history.value().rows.push_back(std::move(history_row));
                }
            }
            catch (std::exception const &e) {
                history = std::unexpected{std::format("Deel History: {}", e.what())};
            }
            catch (...) {
                history = std::unexpected{"Deel History: Unknown error"};
            }
            return history;
        }
        std::function<std::string()> token_provider_;
    };
}