#include "crow_all.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

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

struct Skin {
    std::string name;
    std::string price_text;
    std::string icon_url;
    std::string market_url;
    int price_cents;
    int score;
};

void fetchAndMerge(
    const std::string& query,
    const std::string& sortCol,
    const std::string& sortDir,
    int pages,
    int min_cents,
    int max_cents,
    std::vector<crow::json::wvalue>& results,
    std::set<std::string>& seen
) {
    for (int page = 0; page < pages; page++) {
        std::string url = "https://steamcommunity.com/market/search/render/?query="
            + urlEncode(query)
            + "&appid=730&search_descriptions=0"
            + "&sort_column=" + sortCol
            + "&sort_dir=" + sortDir
            + "&count=10&start=" + std::to_string(page * 10)
            + "&norender=1";

        std::string raw = fetchURL(url);
        if (raw.empty()) continue;

        try {
            auto data = json::parse(raw);
            if (!data.contains("results")) continue;

            for (auto& item : data["results"]) {
                std::string hash = item["hash_name"].get<std::string>();
                if (seen.count(hash)) continue;
                seen.insert(hash);

                int price = item["sell_price"].get<int>();
                if (price < min_cents || price > max_cents) continue;

                crow::json::wvalue skin;
                skin["name"] = item["name"].get<std::string>();
                skin["hash_name"] = hash;
                skin["sell_listings"] = item["sell_listings"].get<int>();
                skin["sell_price"] = price;
                skin["sell_price_text"] = item["sell_price_text"].get<std::string>();
                skin["sale_price_text"] = item["sale_price_text"].get<std::string>();
                skin["icon_url"] = "https://community.akamai.steamstatic.com/economy/image/"
                    + item["asset_description"]["icon_url"].get<std::string>();
                skin["market_url"] = "https://steamcommunity.com/market/listings/730/"
                    + urlEncode(hash);
                results.push_back(std::move(skin));
            }
        } catch (...) {
            continue;
        }
    }
}

void fetchSkinsForBudget(
    const std::string& query,
    const std::string& sortCol,
    const std::string& sortDir,
    int pages,
    int min_cents,
    int max_cents,
    std::vector<Skin>& skins,
    std::set<std::string>& seen
) {
    for (int page = 0; page < pages; page++) {
        std::string url = "https://steamcommunity.com/market/search/render/?query="
            + urlEncode(query)
            + "&appid=730&search_descriptions=0"
            + "&sort_column=" + sortCol
            + "&sort_dir=" + sortDir
            + "&count=10&start=" + std::to_string(page * 10)
            + "&norender=1";

        std::string raw = fetchURL(url);
        if (raw.empty()) continue;

        try {
            auto data = json::parse(raw);
            if (!data.contains("results")) continue;

            for (auto& item : data["results"]) {
                std::string hash = item["hash_name"].get<std::string>();
                if (seen.count(hash)) continue;
                seen.insert(hash);

                int price = item["sell_price"].get<int>();
                int listings = item["sell_listings"].get<int>();
                if (price <= max_cents && price >= min_cents) {
                    skins.push_back({
                        item["name"].get<std::string>(),
                        item["sell_price_text"].get<std::string>(),
                        "https://community.akamai.steamstatic.com/economy/image/"
                            + item["asset_description"]["icon_url"].get<std::string>(),
                        "https://steamcommunity.com/market/listings/730/"
                            + urlEncode(hash),
                        price,
                        listings
                    });
                }
            }
        } catch (...) {
            continue;
        }
    }
}

int main() {
    crow::App<crow::CORSHandler> app;
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .headers("Content-Type")
        .methods("GET"_method, "POST"_method);

    CROW_ROUTE(app, "/")([](){
        return "CS Skin API is running!";
    });

    CROW_ROUTE(app, "/health")([](){
        crow::json::wvalue response;
        response["status"] = "ok";
        response["message"] = "CS Skin API is alive";
        return response;
    });

    // GET /search?q=AK-47&min=10&max=300
    CROW_ROUTE(app, "/search")([](const crow::request& req){
        std::string query = req.url_params.get("q") ? req.url_params.get("q") : "";
        if (query.empty()) {
            crow::json::wvalue error;
            error["error"] = "Missing query parameter ?q=";
            return error;
        }

        std::string min_str = req.url_params.get("min") ? req.url_params.get("min") : "0";
        std::string max_str = req.url_params.get("max") ? req.url_params.get("max") : "999999";
        int min_cents = (int)(std::stod(min_str) * 100);
        int max_cents = (int)(std::stod(max_str) * 100);

        std::vector<crow::json::wvalue> results;
        std::set<std::string> seen;

        fetchAndMerge(query, "popular", "desc", 10, min_cents, max_cents, results, seen);
        fetchAndMerge(query, "price", "desc", 10, min_cents, max_cents, results, seen);

        crow::json::wvalue response;
        response["total_count"] = (int)results.size();
        response["results"] = std::move(results);
        return response;
    });

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

            int budget_cents = (int)(budget * 100);
            int min_price = budget_cents / 10;

            std::vector<Skin> skins;
            std::set<std::string> seen;

            fetchSkinsForBudget(query, "popular", "desc", 10, min_price, budget_cents, skins, seen);
            fetchSkinsForBudget(query, "price", "desc", 10, min_price, budget_cents, skins, seen);

            if (skins.empty()) {
                crow::json::wvalue error;
                error["error"] = "No skins found within budget. Try a more specific search like 'AK-47 Redline'";
                return error;
            }

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

            std::vector<crow::json::wvalue> selected;
            int w = budget_cents;
            int total_spent = 0;
            for (int i = n; i > 0; i--) {
                if (dp[i][w] != dp[i-1][w]) {
                    crow::json::wvalue skin;
                    skin["name"] = skins[i-1].name;
                    skin["price"] = skins[i-1].price_text;
                    skin["listings"] = skins[i-1].score;
                    skin["icon_url"] = skins[i-1].icon_url;
                    skin["market_url"] = skins[i-1].market_url;
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