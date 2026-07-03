// Capstone program entry point.
//
// Drive the Bitcoin Core regtest node through the full workflow and write the
// 10-line out.txt at the repo root.
//
// Workflow to implement (see README.md for details):
//   1. Create/load the "Miner" and "Trader" wallets (idempotent).
//   2. Generate a Miner address (label "Mining Reward") and mine 101 blocks to
//      it so the first coinbase reward matures (100-confirmation rule).
//   3. Print the Miner balance.
//   4. Generate a Trader receiving address (label "Received").
//   5. Send 20 BTC from Miner to Trader.
//   6. Confirm the transaction is in the mempool.
//   7. Mine 1 block to confirm it.
//   8. Fetch transaction details (gettransaction / getrawtransaction /
//      decoderawtransaction) and write the report to out.txt.

#include "main.h"
#include "rpc_client.h"

#include <curl/curl.h>

#include <iostream>
#include <fstream>

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    int exit_code = 0;
    try {
        RpcConfig cfg;  // defaults: alice:password @ 127.0.0.1:18443
        BitcoinRPC client(cfg);

        // TODO: implement the capstone workflow described above and write out.txt.

        //   1. Create/load the "Miner" and "Trader" wallets (idempotent).
        std::array<std::string, 2> target_wallets = {"Miner", "Trader"};
        auto on_disk = client.call("listwalletdir");
        auto loaded = client.call("listwallets");

        std::vector<std::string> on_disk_wallets;
        for (const auto& wallet : on_disk["wallets"]) {
            on_disk_wallets.push_back(
                wallet.at("name").get<std::string>());
        }

        std::vector<std::string> loaded_wallets;
        for (const auto& wallet : loaded) {
            loaded_wallets.push_back(wallet.get<std::string>());
        }

        for (const auto& wallet : target_wallets) {
            auto action = decide_wallet_action(on_disk_wallets, loaded_wallets, wallet);
            switch (action) {
                case WalletAction::Create:
                    std::cout << "Creating wallet: " << wallet << "\n";
                    client.call("createwallet", {wallet});
                    break;
                case WalletAction::Load:
                    std::cout << "Loading wallet: " << wallet << "\n";
                    client.call("loadwallet", {wallet});
                    break;
                case WalletAction::None:
                    std::cout << "Wallet is already loaded: " << wallet << "\n";
                    break;
            }
        }

        //   2. Generate a Miner address (label "Mining Reward") and mine 101 blocks to
        //      it so the first coinbase reward matures (100-confirmation rule).
        auto miner_address_json = client.wallet_call(target_wallets[0],"getnewaddress", {"Mining Reward"});
        std::string miner_address = miner_address_json.get<std::string>();
        std::cout << "Miner address: " << miner_address << "\n";
        client.call("generatetoaddress", {101, miner_address});


        //   3. Print the Miner balance.
        auto miner_balance_json = client.wallet_call(target_wallets[0], "getbalance");
        auto miner_balance = miner_balance_json.get<double>();
        std::cout << "Miner balance: " << miner_balance << "\n";
        //   4. Generate a Trader receiving address (label "Received").
        auto trader_address_json = client.wallet_call(target_wallets[1], "getnewaddress", {"Received"});
        std::string trader_address = trader_address_json.get<std::string>();
        std::cout << "Trader address: " << trader_address << "\n";

        //   5. Send 20 BTC from Miner to Trader.
        auto send_tx_json = client.wallet_call(target_wallets[0], "sendtoaddress", {trader_address, 20.0});
        std::string txid = send_tx_json.get<std::string>();
        std::cout << "Sent to Trader Transaction ID: " << txid << "\n";

        //   6. Confirm the transaction is in the mempool.
        bool in_mempool = false;
        auto mempool_json = client.call("getrawmempool");
        auto it = std::find(mempool_json.begin(), mempool_json.end(), txid);

        if (it != mempool_json.end()) {
            in_mempool = true;
        }
        std::cout << "Transaction in mempool: " << (in_mempool ? "yes" : "no") << "\n";
        if (!in_mempool) {
            throw std::runtime_error("Transaction not found in mempool");
        }

        //   7. Mine 1 block to confirm it.
        client.call("generatetoaddress", {1, miner_address});

        //   8. Fetch transaction details (gettransaction / getrawtransaction /
        //      decoderawtransaction) and write the report to out.txt.
        auto tx_details_json = client.wallet_call(target_wallets[0], "gettransaction", {txid});
        double fee = tx_details_json.at("fee").get<double>();
        std::string blockhash = tx_details_json.at("blockhash").get<std::string>();
        int blockheight = tx_details_json.at("blockheight").get<int>();
        std::string hex = tx_details_json.at("hex").get<std::string>();
        std::cout << "Transaction fee: " << fee << "\n";
        std::cout << "Block hash: " << blockhash << "\n";
        std::cout << "Block height: " << blockheight << "\n";
        std::cout << "Hex: " << hex << "\n";

        auto decoded_tx_json = client.wallet_call(target_wallets[0], "decoderawtransaction", {hex});

        auto vouts = parse_vouts(decoded_tx_json);

        auto trader_vout = select_recipient_vout(vouts, trader_address);
        std::cout << "Trader vout: " << trader_vout.value << " (n=" << trader_vout.n << ")\n";

        auto change_output = select_change_vout(vouts, trader_address);
        std::cout << "Change output: " << change_output.value << " (n=" << change_output.n << ")\n";

        const auto& vin = decoded_tx_json.at("vin").at(0);
        std::string prev_txid = vin.at("txid").get<std::string>();
        long long prev_vout = vin.at("vout").get<long long>();

        auto prev_hex = client.call("getrawtransaction", {prev_txid});
        auto prev_decoded = client.call("decoderawtransaction", {prev_hex.get<std::string>()});
        auto miner_input = resolve_input_prevout(prev_decoded, prev_vout);
        std::cout << "Miner input: " << miner_input.address << " (value=" << miner_input.value << ")\n";

        TxReport report;
        report.txid = txid;
        report.miner_input_address = miner_input.address;
        report.miner_input_amount = miner_input.value;
        report.trader_output_address = trader_vout.address;
        report.trader_output_amount = trader_vout.value;
        report.miner_change_address = change_output.address;
        report.miner_change_amount = change_output.value;
        report.fee = fee;
        report.block_height = blockheight;
        report.block_hash = blockhash;
        std::string output = format_report(report);
        std::cout << "Report: " << output << "\n";

        std::ofstream out("out.txt");
        if (!out) {
          throw std::runtime_error("Failed to open out.txt");
        }
        out << output;

    } catch (const std::exception& e) {
        std::cerr << "Error occurred: " << e.what() << "\n";
        exit_code = 1;
    }

    // curl_global_cleanup();
    return exit_code;
}
