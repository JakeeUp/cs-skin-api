#include "crow_all.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>

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

    // Budget optimizer - knapsack algorithm
    // POST /budget/optimize
    // Body: {"budget": 100.00, "query": "AK-47"}
    CROW_ROUTE(app, "/budget/optimize").methods(crow::HTTPMethod::Post)([](const crow::request& req){
        try {
            auto body = json::parse(req.body);
            double budget = body.value("budget", 0.0);
            std::string query = body.value("query", "");

            if (budget <= 0 || query.empty()) {
                crow::json::wvalue error;
                error["error"] = "Missing budget or query";
                return error;
            }

            // Fetch skins matching query
            std::string url = "https://steamcommunity.com/market/search/render/?query="
                + urlEncode(query)
                + "&appid=730&search_descriptions=0&sort_column=popular&sort_dir=desc&norender=1";

            std::string raw = fetchURL(url);
            auto data = json::parse(raw);

            struct Skin {
                std::string name;
                std::string price_text;
                int price_cents;
                int score;
            };

            std::vector<Skin> skins;
            int budget_cents = (int)(budget * 100);

            for (auto& item : data["results"]) {
                int price = item["sell_price"].get<int>();
                int listings = item["sell_listings"].get<int>();
                // Filter out skins under $1 to avoid cheap junk
                if (price <= budget_cents && price >= 100) {
                    skins.push_back({
                        item["name"].get<std::string>(),
                        item["sell_price_text"].get<std::string>(),
                        price,
                        listings
                    });
                }
            }

            // DEBUG
            std::cout << "Total results from Steam: " << data["results"].size() << std::endl;
            std::cout << "Budget cents: " << budget_cents << std::endl;
            for (auto& item : data["results"]) {
                std::cout << "Skin: " << item["name"] << " Price: " << item["sell_price"] << std::endl;
            }
            std::cout << "Skins passing filter: " << skins.size() << std::endl;

            // Knapsack - maximize score (popularity) within budget
            int n = skins.size();
            std::vector<std::vector<int>> dp(n + 1, std::vector<int>(budget_cents + 1, 0));

            for (int i = 1; i <= n; i++) {
                for (int w = 0; w <= budget_cents; w++) {
                    dp[i][w] = dp[i-1][w];
                    if (skins[i-1].price_cents <= w) {
                        dp[i][w] = std::max(dp[i][w], dp[i-1][w - skins[i-1].price_cents] + skins[i-1].score);
                    }
                }
            }

            // Backtrack to find selected skins
            std::vector<crow::json::wvalue> selected;
            int w = budget_cents;
            int total_spent = 0;
            for (int i = n; i > 0; i--) {
                if (dp[i][w] != dp[i-1][w]) {
                    crow::json::wvalue skin;
                    skin["name"] = skins[i-1].name;
                    skin["price"] = skins[i-1].price_text;
                    skin["listings"] = skins[i-1].score;
                    selected.push_back(std::move(skin));
                    w -= skins[i-1].price_cents;
                    total_spent += skins[i-1].price_cents;
                }
            }

            crow::json::wvalue response;
            response["budget"] = budget;
            response["total_spent"] = (double)total_spent / 100.0;
            response["remaining"] = (double)(budget_cents - total_spent) / 100.0;
            response["skins"] = std::move(selected);
            return response;

        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return error;
        }
    });

    app.port(8080).multithreaded().run();
}
