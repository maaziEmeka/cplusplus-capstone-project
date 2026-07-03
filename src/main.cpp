// Pure helper functions for the capstone (no network / no I/O).
//
// These are unit-tested by tests/main_test.cpp without a running Bitcoin node.
// Implement each TODO so that the GoogleTest suite passes.

#include "main.h"

#include <algorithm>
#include <stdexcept>
#include <string>

std::string build_base_url(const RpcConfig& cfg) {
    // TODO: return "<scheme>://<host>:<port>", e.g. "http://127.0.0.1:18443".
    return cfg.scheme + "://" + cfg.host + ":" + std::to_string(cfg.port);
}

std::string build_wallet_url(const RpcConfig& cfg, const std::string& wallet) {
    // TODO: return the base URL with "/wallet/<wallet>" appended.
    return build_base_url(cfg) + "/wallet/" + wallet;
}

nlohmann::json build_rpc_request(const std::string& method,
                                 const nlohmann::json& params,
                                 const std::string& id) {
    // TODO: build {"jsonrpc":"1.0","id":id,"method":method,"params":params}.
    nlohmann::json request;
    request["jsonrpc"] = "1.0";
    request["id"] = id;
    request["method"] = method;
    request["params"] = params;
    return request;
}

WalletAction decide_wallet_action(const std::vector<std::string>& on_disk,
                                  const std::vector<std::string>& loaded,
                                  const std::string& wallet) {
    // TODO: Create if not on disk; Load if on disk but not loaded; else None.

    bool exists =
        std::find(on_disk.begin(), on_disk.end(), wallet) != on_disk.end();

    bool is_loaded =
        std::find(loaded.begin(), loaded.end(), wallet) != loaded.end();

    if (!exists)
        return WalletAction::Create;

    if (!is_loaded)
        return WalletAction::Load;

    return WalletAction::None;
}

std::vector<VoutEntry> parse_vouts(const nlohmann::json& decoded_tx) {
    // TODO: read each entry of decoded_tx["vout"] into a VoutEntry (value, n,
    //       and scriptPubKey.address with fallback to addresses[0]).
     std::vector<VoutEntry> result;

       for (const auto& vout : decoded_tx.at("vout"))
       {
           VoutEntry entry;

           entry.value = vout.at("value").get<double>();
           entry.n = vout.at("n").get<long long>();

           const auto& spk = vout.at("scriptPubKey");

           if (spk.contains("address"))
           {
               entry.address = spk.at("address").get<std::string>();
           }
           else if (spk.contains("addresses"))
           {
               entry.address = spk.at("addresses").at(0).get<std::string>();
           }

           result.push_back(entry);
       }

       return result;
}

VoutEntry select_recipient_vout(const std::vector<VoutEntry>& vouts,
                                const std::string& trader_address) {
    // TODO: return the output whose address == trader_address (throw if none).
    for (const auto& vout : vouts) {
        if (vout.address == trader_address) {
            return vout;
        }
    }
    throw std::runtime_error("no recipient vout found");
}

VoutEntry select_change_vout(const std::vector<VoutEntry>& vouts,
                             const std::string& trader_address) {
    // TODO: return the output whose address != trader_address (throw if none).
    for (const auto& vout : vouts) {
        if (vout.address != trader_address) {
            return vout;
        }
    }
    throw std::runtime_error("no change vout found");
}

VoutEntry resolve_input_prevout(const nlohmann::json& prev_decoded,
                                long long input_vout) {
    // TODO: index prev_decoded["vout"] at input_vout (throw if out of range).
    try
       {
           const auto& vout = prev_decoded.at("vout").at(input_vout);

           VoutEntry entry;
           entry.value = vout.at("value").get<double>();
           entry.n = input_vout;

           const auto& spk = vout.at("scriptPubKey");

           if (spk.contains("address"))
               entry.address = spk.at("address").get<std::string>();
           else if (spk.contains("addresses"))
               entry.address = spk.at("addresses").at(0).get<std::string>();

           return entry;
       }
       catch (const nlohmann::json::out_of_range&)
       {
           throw std::runtime_error("input_vout out of range");
       }
}

std::string format_btc(double amount) {
    // TODO: shortest round-trip representation, trailing ".0" stripped.
    std::ostringstream oss;
       oss << std::fixed << std::setprecision(8) << amount;

       std::string result = oss.str();

       while (!result.empty() && result.back() == '0')
       {
           result.pop_back();
       }

       if (!result.empty() && result.back() == '.')
       {
           result.pop_back();
       }

       return result;
}

std::string format_report(const TxReport& r) {
    // TODO: produce the 10 out.txt lines in order, one per line.
    std::ostringstream oss;
    oss << r.txid << "\n"
        << r.miner_input_address << "\n"
        << format_btc(r.miner_input_amount) << "\n"
        << r.trader_output_address << "\n"
        << format_btc(r.trader_output_amount) << "\n"
        << r.miner_change_address << "\n"
        << format_btc(r.miner_change_amount) << "\n"
        << format_btc(r.fee) << "\n"
        << r.block_height << "\n"
        << r.block_hash << "\n";
    return oss.str();
}
