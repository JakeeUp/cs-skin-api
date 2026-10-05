#include "crow_all.h"
#include "http_client.hpp"
#include "optimizer.hpp"
#include "steam_market.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using json = nlohmann::json;

// Full Steam search response shape (used by /search).
static crow::json::wvalue skinToJson(const Skin& s) {
    crow::json::wvalue j;
    j["name"]            = s.name;
    j["hash_name"]       = s.hash_name;
    j["sell_price"]      = s.price_cents;
    j["sell_price_text"] = s.price_text;
    j["sale_price_text"] = s.sale_price_text;
    j["sell_listings"]   = s.listings;
    j["icon_url"]        = s.icon_url;
    j["market_url"]      = s.market_url;
    return j;
}

// Compact shape used by /budget/optimize and /loadout/build.
static crow::json::wvalue skinOptionJson(const Skin& s) {
    crow::json::wvalue o;
    o["name"]        = s.name;
    o["price"]       = s.price_text;
    o["price_cents"] = s.price_cents;
    o["listings"]    = s.listings;
    o["icon_url"]    = s.icon_url;
    o["market_url"]  = s.market_url;
    return o;
}

static std::vector<crow::json::wvalue> toOptionList(const std::vector<Skin>& skins) {
    std::vector<crow::json::wvalue> out;
    out.reserve(skins.size());
    for (const auto& s : skins) out.push_back(skinOptionJson(s));
    return out;
}

int main() {
    crow::App<crow::CORSHandler> app;
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .headers("Content-Type")
        .methods("GET"_method, "POST"_method);

    // GET /
    CROW_ROUTE(app, "/")([]() {
        return "CS Skin API is running!";
    });

    // GET /health
    CROW_ROUTE(app, "/health")([]() {
        crow::json::wvalue r;
        r["status"]  = "ok";
        r["message"] = "CS Skin API is alive";
        return r;
    });

    // GET /search?q=AK-47&min=0&max=300
    CROW_ROUTE(app, "/search")([](const crow::request& req) {
        std::string query = req.url_params.get("q") ? req.url_params.get("q") : "";
        if (query.empty()) {
            crow::json::wvalue e;
            e["error"] = "Missing query parameter ?q=";
            return e;
        }

        double min_d = req.url_params.get("min") ? std::stod(req.url_params.get("min")) : 0.0;
        double max_d = req.url_params.get("max") ? std::stod(req.url_params.get("max")) : 999999.0;
        int min_cents = static_cast<int>(std::max(0.0, min_d) * 100);
        int max_cents = static_cast<int>(std::max(0.0, max_d) * 100);

        std::vector<Skin>     skins;
        std::set<std::string> seen;

        fetchQuery(query, 10, min_cents, max_cents, skins, seen);

        std::vector<crow::json::wvalue> results;
        results.reserve(skins.size());
        for (const auto& s : skins)
            results.push_back(skinToJson(s));

        crow::json::wvalue r;
        r["total_count"] = static_cast<int>(results.size());
        r["results"]     = std::move(results);
        return r;
    });

    // GET /price?name=AK-47+Redline+(Field-Tested)
    CROW_ROUTE(app, "/price")([](const crow::request& req) {
        std::string name = req.url_params.get("name") ? req.url_params.get("name") : "";
        if (name.empty()) {
            crow::json::wvalue e;
            e["error"] = "Missing name parameter ?name=";
            return e;
        }

        std::string url =
            "https://steamcommunity.com/market/priceoverview/?appid=730&currency=1"
            "&market_hash_name=" + urlEncode(name);

        std::string raw = fetchURL(url);

        try {
            auto data = json::parse(raw);
            crow::json::wvalue r;
            r["name"]         = name;
            r["lowest_price"] = data.value("lowest_price", "N/A");
            r["median_price"] = data.value("median_price", "N/A");
            r["volume"]       = data.value("volume", "N/A");
            return r;
        } catch (const std::exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return err;
        }
    });

    // POST /budget/optimize
    // Body: { "budget": 50.00, "query": "AK-47" }
    CROW_ROUTE(app, "/budget/optimize").methods(crow::HTTPMethod::Post)([](const crow::request& req) {
        try {
            auto body = json::parse(req.body);
            double budget     = body.value("budget", 0.0);
            std::string query = body.value("query", "");

            if (budget <= 0 || query.empty()) {
                crow::json::wvalue e;
                e["error"] = "Missing or invalid budget/query";
                return e;
            }

            if (budget > 10000.0) {
                crow::json::wvalue e;
                e["error"] = "Budget cannot exceed $10,000";
                return e;
            }

            int budget_cents = static_cast<int>(budget * 100);

            std::vector<Skin>     skins;
            std::set<std::string> seen;
            fetchQuery(query, 10, 1, budget_cents, skins, seen);

            if (skins.empty()) {
                crow::json::wvalue e;
                e["error"] = "No skins found within budget.";
                return e;
            }

            // Run knapsack to find the optimal combination within budget
            auto selected = knapsackOptimize(skins, budget_cents);

            int total_cents = 0;
            for (const auto& s : selected) total_cents += s.price_cents;
            double total_spent = total_cents / 100.0;

            crow::json::wvalue r;
            r["budget"]         = budget;
            r["total_spent"]    = total_spent;
            r["remaining"]      = budget - total_spent;
            r["skins_found"]    = static_cast<int>(skins.size());
            r["skins_selected"] = static_cast<int>(selected.size());
            r["algorithm"]      = usesGreedy(budget_cents, static_cast<int>(skins.size()))
                                   ? "greedy" : "knapsack_dp";
            r["skins"]          = toOptionList(selected);
            return r;

        } catch (const std::exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return err;
        }
    });

    // POST /loadout/build
    // Body:
    // {
    //   "side":           "T" | "CT",
    //   "weapons_budget": 100.00,   -- split evenly across primary + secondary
    //   "knife_budget":    50.00,   -- 0 or omitted = skip
    //   "gloves_budget":   30.00    -- 0 or omitted = skip
    // }
    CROW_ROUTE(app, "/loadout/build").methods(crow::HTTPMethod::Post)([](const crow::request& req) {
        try {
            auto body = json::parse(req.body);

            std::string side      = body.value("side",           "T");
            double weapons_budget = body.value("weapons_budget", 0.0);
            double knife_budget   = body.value("knife_budget",   0.0);
            double gloves_budget  = body.value("gloves_budget",  0.0);

            if (side != "T" && side != "CT") {
                crow::json::wvalue e;
                e["error"] = "side must be 'T' or 'CT'";
                return e;
            }

            if (weapons_budget <= 0) {
                crow::json::wvalue e;
                e["error"] = "weapons_budget must be greater than 0";
                return e;
            }

            int per_weapon_cents = static_cast<int>((weapons_budget / 2.0) * 100);
            int knife_cents      = static_cast<int>(knife_budget  * 100);
            int gloves_cents     = static_cast<int>(gloves_budget * 100);

            std::cerr << "[loadout/build] side=" << side
                      << " weapons=" << weapons_budget
                      << " knife="   << knife_budget
                      << " gloves="  << gloves_budget << std::endl;

            // Weapon lists per side
            std::vector<std::string> primary_queries;
            std::vector<std::string> secondary_queries;

            if (side == "CT") {
                primary_queries   = {"M4A4", "M4A1-S", "AUG", "FAMAS"};
                secondary_queries = {"USP-S", "P2000", "Five-SeveN", "P250"};
            } else {
                primary_queries   = {"AK-47", "SG 553", "Galil AR"};
                secondary_queries = {"Glock-18", "Tec-9", "Desert Eagle"};
            }

            crow::json::wvalue slots;
            auto addSlot = [&](const char* key, const std::vector<std::string>& queries, int cents) {
                auto opts = fetchSlotOptions(queries, cents, 5);
                if (!opts.empty()) slots[key] = toOptionList(opts);
            };

            addSlot("primary",   primary_queries,   per_weapon_cents);
            addSlot("secondary", secondary_queries, per_weapon_cents);
            if (knife_cents  > 0) addSlot("knife",  {"Knife"},  knife_cents);
            if (gloves_cents > 0) addSlot("gloves", {"Gloves"}, gloves_cents);

            crow::json::wvalue r;
            r["side"]           = side;
            r["weapons_budget"] = weapons_budget;
            r["knife_budget"]   = knife_budget;
            r["gloves_budget"]  = gloves_budget;
            r["slots"]          = std::move(slots);
            return r;

        } catch (const std::exception& e) {
            crow::json::wvalue err;
            err["error"] = e.what();
            return err;
        }
    });

    app.port(8080).multithreaded().run();
}
