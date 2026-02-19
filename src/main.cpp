#include "crow_all.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <string>
#include <iostream>

using json = nlohmann::json;

// Curl write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// URL encode a string using curl
std::string urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    std::string encoded;
    if (curl) {
        char* output = curl_easy_escape(curl, str.c_str(), str.length());
        if (output) {
            encoded = output;
            curl_free(output);
        }
        curl_easy_cleanup(curl);
    }
    return encoded;
}

// Fetch a URL and return the response body
std::string fetchURL(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl error: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }
    return response;
}

int main() {
    crow::SimpleApp app;

    // Root
    CROW_ROUTE(app, "/")([](){
        return "CS Skin API is running!";
    });

    // Health check
    CROW_ROUTE(app, "/health")([](){
        crow::json::wvalue response;
        response["status"] = "ok";
        response["message"] = "CS Skin API is alive";
        return response;
    });

    // Search skins by name
    // GET /search?q=AK-47+Redline
    CROW_ROUTE(app, "/search")([](const crow::request& req){
        std::string query = req.url_params.get("q") ? req.url_params.get("q") : "";

        if (query.empty()) {
            crow::json::wvalue error;
            error["error"] = "Missing query parameter ?q=";
            return error;
        }

        std::string url = "https://steamcommunity.com/market/search/render/?query="
            + urlEncode(query)
            + "&appid=730&search_descriptions=0&sort_column=popular&sort_dir=desc&norender=1";

        std::string raw = fetchURL(url);

        try {
            auto data = json::parse(raw);
            crow::json::wvalue response;
            response["total_count"] = data["total_count"].get<int>();

            std::vector<crow::json::wvalue> results;
            for (auto& item : data["results"]) {
                crow::json::wvalue skin;
                skin["name"] = item["name"].get<std::string>();
                skin["hash_name"] = item["hash_name"].get<std::string>();
                skin["sell_listings"] = item["sell_listings"].get<int>();
                skin["sell_price"] = item["sell_price"].get<int>();
                skin["sell_price_text"] = item["sell_price_text"].get<std::string>();
                skin["sale_price_text"] = item["sale_price_text"].get<std::string>();
                results.push_back(std::move(skin));
            }
            response["results"] = std::move(results);
            return response;

        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return error;
        }
    });

    // Get price for a specific skin
    // GET /price?name=AK-47+Redline+(Field-Tested)
    CROW_ROUTE(app, "/price")([](const crow::request& req){
        std::string name = req.url_params.get("name") ? req.url_params.get("name") : "";

        if (name.empty()) {
            crow::json::wvalue error;
            error["error"] = "Missing name parameter ?name=";
            return error;
        }

        std::string url = "https://steamcommunity.com/market/priceoverview/?appid=730&currency=1&market_hash_name="
            + urlEncode(name);

        std::string raw = fetchURL(url);

        try {
            auto data = json::parse(raw);
            crow::json::wvalue response;
            response["name"] = name;
            response["lowest_price"] = data.value("lowest_price", "N/A");
            response["median_price"] = data.value("median_price", "N/A");
            response["volume"] = data.value("volume", "N/A");
            return response;

        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return error;
        }
    });

    app.port(8080).multithreaded().run();
}