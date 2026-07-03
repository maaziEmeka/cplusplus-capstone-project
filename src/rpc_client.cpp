// JSON-RPC client for Bitcoin Core, backed by libcurl.
//
// Implement each TODO so the program can talk to the node.

#include "rpc_client.h"

#include <curl/curl.h>

#include <stdexcept>
#include <string>
#include <utility>

BitcoinRPC::BitcoinRPC(RpcConfig cfg) : cfg_(std::move(cfg)) {
    // TODO: initialise a libcurl easy handle into curl_ (throw on failure).
    try {
        if(curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            throw std::runtime_error("curl_global_init failed");
        }
        curl_ = curl_easy_init();
        if (!curl_) {
            throw std::runtime_error("curl_easy_init handle failed");
        }
        if (curl_easy_setopt(curl_, CURLOPT_URL, build_base_url(cfg_).c_str()) != CURLE_OK) {
            throw std::runtime_error("curl_easy_setopt url failed");
        }
        if (curl_easy_setopt(curl_, CURLOPT_USERNAME, cfg_.user.c_str()) != CURLE_OK) {
            throw std::runtime_error("curl_easy_setopt username failed");
        }
    if (curl_easy_setopt(curl_, CURLOPT_PASSWORD, cfg_.pass.c_str()) != CURLE_OK) {
            throw std::runtime_error("curl_easy_setopt password failed");
        }
    } catch (...) {
        curl_easy_cleanup(static_cast<CURL*>(curl_));
        throw;
    }
}

BitcoinRPC::~BitcoinRPC() {
    // TODO: clean up the libcurl easy handle if non-null.
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
    curl_global_cleanup();
}

BitcoinRPC::BitcoinRPC(BitcoinRPC&& other) noexcept
    : cfg_(std::move(other.cfg_)), curl_(other.curl_) {
    other.curl_ = nullptr;
}

BitcoinRPC& BitcoinRPC::operator=(BitcoinRPC&& other) noexcept {
    // TODO: free any existing handle, then steal other's handle/config.
    if (this == &other) return *this;
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
    curl_ = other.curl_;
    other.curl_ = nullptr;
    cfg_ = std::move(other.cfg_);
    return *this;
}

size_t BitcoinRPC::write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    // TODO: append (size * nmemb) bytes from ptr to the std::string in userdata
    //       and return the number of bytes handled.
    std::string* str = static_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

nlohmann::json BitcoinRPC::post(const std::string& url, const nlohmann::json& body) {
    // TODO: POST body.dump() to url with a JSON content-type header and HTTP
    //       basic auth; capture the response via write_cb; parse it; raise on a
    //       non-null JSON-RPC "error"; return the "result" field.
    std::string payload = body.dump();
    std::string response;
    CURL* curl = static_cast<CURL*>(curl_);
    curl_slist* headers = nullptr;
    try {
        if(curl_easy_setopt(curl, CURLOPT_URL, url.c_str()) != CURLE_OK) {
            throw std::runtime_error("Failed to set curl URL");
        }
        if(curl_easy_setopt(curl, CURLOPT_POST, 1L) != CURLE_OK) {
            throw std::runtime_error("Failed to set curl POST");
        }
        if(curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str()) != CURLE_OK) {
            throw std::runtime_error("Failed to set curl POSTFIELDS");
        }
        headers = curl_slist_append(headers, "Content-Type: application/json");
        if(!headers) {
            throw std::runtime_error("Failed to create curl headers");
        }
        if(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK) {
            throw std::runtime_error("Failed to set curl HTTPHEADER");
        }

        if(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb) != CURLE_OK) {
            throw std::runtime_error("Failed to set curl WRITEFUNCTION");
        }
        if(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response) != CURLE_OK) {
            throw std::runtime_error("Failed to set curl WRITEDATA");
        }
        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            throw std::runtime_error(curl_easy_strerror(res));
        }
        nlohmann::json reply = nlohmann::json::parse(response);
        if(!reply.at("error").is_null()) {
            throw std::runtime_error(reply.at("error").at("message").get<std::string>());
        }
        curl_slist_free_all(headers);
        return reply.at("result");

    } catch (...) {
        curl_slist_free_all(headers);
        throw ;
    }
}

nlohmann::json BitcoinRPC::call(const std::string& method, const nlohmann::json& params) {
    // TODO: post(build_base_url(cfg_), build_rpc_request(method, params)).
    return post(build_base_url(cfg_), build_rpc_request(method, params));
}

nlohmann::json BitcoinRPC::wallet_call(const std::string& wallet,
                                       const std::string& method,
                                       const nlohmann::json& params) {
    // TODO: post(build_wallet_url(cfg_, wallet), build_rpc_request(method, params)).

    return post(build_wallet_url(cfg_, wallet), build_rpc_request(method, params));
}
