#include "crow_all.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

using json = nlohmann::json;

// ─── CURL Helpers ──────────────────────────────────────────

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    std::string encoded;
    if (curl) {
        char* out = curl_easy_escape(curl, str.c_str(), (int)str.length());
        if (out) {
            encoded = out;
            curl_free(out);
        }
        curl_easy_cleanup(curl);
    }
    return encoded;
}

std::string fetchURL(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;
    if (!curl) {
        std::cerr << "[fetchURL] Failed to init CURL" << std::endl;
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    // Steam requires these headers to not block requests
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
    headers = curl_slist_append(headers, "Accept: application/json, text/javascript, */*; q=0.01");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[fetchURL] CURL error: " << curl_easy_strerror(res)
                  << " | URL: " << url << std::endl;
        return "";
    }

    return response;
}

// ─── Skin Struct ───────────────────────────────────────────

struct Skin {
    std::string name;
    std::string price_text;
    std::string icon_url;
    std::string market_url;
    int         price_cents;
    int         listings;
};

// ─── Core Steam Market Fetch ───────────────────────────────

// Fetches one page of Steam market results for a query.
// Appends valid skins (price within range) into `skins`, deduplicating via `seen`.
void fetchPage(
    const std::string&     query,
    const std::string&     sortCol,
    const std::string&     sortDir,
    int                    start,
    int                    min_cents,
    int                    max_cents,
    std::vector<Skin>&     skins,
    std::set<std::string>& seen
) {
    std::string url =
        "https://steamcommunity.com/market/search/render/?appid=730"
        "&search_descriptions=0&norender=1"
        "&count=10"
        "&start="      + std::to_string(start)   +
        "&sort_column=" + sortCol                 +
        "&sort_dir="    + sortDir                 +
        "&query="       + urlEncode(query);

    std::cerr << "[fetchPage] " << query << " | start=" << start
              << " | budget=" << max_cents << "c" << std::endl;

    std::string raw = fetchURL(url);

    if (raw.empty()) {
        std::cerr << "[fetchPage] Empty response for: " << query << std::endl;
        return;
    }

    // Steam sometimes returns HTML error pages — check for JSON
    if (raw.front() != '{' && raw.front() != '[') {
        std::cerr << "[fetchPage] Non-JSON response (" << raw.length()
                  << " bytes) for: " << query << std::endl;
        return;
    }

    try {
        auto data = json::parse(raw);

        if (!data.contains("results") || !data["results"].is_array()) {
            std::cerr << "[fetchPage] No results array in response for: " << query << std::endl;
            return;
        }

        int added = 0;
        for (auto& item : data["results"]) {
            // Guard all field accesses
            if (!item.contains("hash_name") || !item.contains("sell_price") ||
                !item.contains("sell_listings") || !item.contains("name") ||
                !item.contains("sell_price_text") || !item.contains("asset_description"))
                continue;

            std::string hash = item["hash_name"].get<std::string>();
            if (seen.count(hash)) continue;

            int price    = item["sell_price"].get<int>();
            int listings = item["sell_listings"].get<int>();

            if (price <= 0 || price < min_cents || price > max_cents) continue;

            auto& desc = item["asset_description"];
            if (!desc.contains("icon_url")) continue;

            seen.insert(hash);
            skins.push_back({
                item["name"].get<std::string>(),
                item["sell_price_text"].get<std::string>(),
                "https://community.akamai.steamstatic.com/economy/image/"
                    + desc["icon_url"].get<std::string>(),
                "https://steamcommunity.com/market/listings/730/"
                    + urlEncode(hash),
                price,
                listings
            });
            added++;
        }

        std::cerr << "[fetchPage] Added " << added << " skins for: " << query << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[fetchPage] JSON parse error: " << e.what()
                  << " | query: " << query << std::endl;
    }
}

// Fetches multiple pages for a query across two sort orders (popular + price)
void fetchQuery(
    const std::string&     query,
    int                    pages,
    int                    min_cents,
    int                    max_cents,
    std::vector<Skin>&     skins,
    std::set<std::string>& seen
) {
    for (int p = 0; p < pages; p++) {
        fetchPage(query, "popular", "desc", p * 10, min_cents, max_cents, skins, seen);
        fetchPage(query, "price",   "desc", p * 10, min_cents, max_cents, skins, seen);
    }
}

// Fetches options per weapon query, takes the best result from each weapon,
// then fills remaining slots with next-best across all weapons.
// This ensures variety — e.g. one AK-47, one SG 553, one Galil AR — rather than
// all slots going to whichever weapon has the most cheap listings.
std::vector<crow::json::wvalue> fetchSlotOptions(
    const std::vector<std::string>& queries,
    int                             budget_cents,
    int                             max_options = 5
) {
    // Fetch best results per weapon separately
    std::vector<std::vector<Skin>> perWeapon;
    std::set<std::string> globalSeen;

    for (const auto& q : queries) {
        std::vector<Skin>     weaponSkins;
        std::set<std::string> weaponSeen;
        fetchQuery(q, 3, 1, budget_cents, weaponSkins, weaponSeen);

        // Sort each weapon's results by price desc
        std::sort(weaponSkins.begin(), weaponSkins.end(), [](const Skin& a, const Skin& b) {
            return a.price_cents > b.price_cents;
        });

        // Deduplicate against global seen
        std::vector<Skin> filtered;
        for (auto& s : weaponSkins) {
            if (!globalSeen.count(s.market_url)) {
                globalSeen.insert(s.market_url);
                filtered.push_back(s);
            }
        }

        if (!filtered.empty())
            perWeapon.push_back(std::move(filtered));
    }

    // Interleave: first pick the best (index 0) from each weapon round-robin,
    // then pick the second-best (index 1), and so on until max_options filled
    std::vector<Skin> interleaved;
    size_t maxDepth = 0;
    for (auto& w : perWeapon)
        if (w.size() > maxDepth) maxDepth = w.size();

    for (size_t depth = 0; depth < maxDepth && (int)interleaved.size() < max_options; depth++) {
        for (auto& w : perWeapon) {
            if ((int)interleaved.size() >= max_options) break;
            if (depth < w.size())
                interleaved.push_back(w[depth]);
        }
    }

    // Build final output
    std::vector<crow::json::wvalue> options;
    for (auto& s : interleaved) {
        crow::json::wvalue o;
        o["name"]        = s.name;
        o["price"]       = s.price_text;
        o["price_cents"] = s.price_cents;
        o["listings"]    = s.listings;
        o["icon_url"]    = s.icon_url;
        o["market_url"]  = s.market_url;
        options.push_back(std::move(o));
    }

    std::cerr << "[fetchSlotOptions] Returning " << options.size()
              << " options across " << perWeapon.size()
              << " weapons (budget=" << budget_cents << "c)" << std::endl;

    return options;
}

// ─── /search variant — returns crow::json::wvalue directly ─

void fetchAndMerge(
    const std::string&                  query,
    const std::string&                  sortCol,
    const std::string&                  sortDir,
    int                                 pages,
    int                                 min_cents,
    int                                 max_cents,
    std::vector<crow::json::wvalue>&    results,
    std::set<std::string>&              seen
) {
    for (int page = 0; page < pages; page++) {
        std::string url =
            "https://steamcommunity.com/market/search/render/?appid=730"
            "&search_descriptions=0&norender=1"
            "&count=10"
            "&start="       + std::to_string(page * 10) +
            "&sort_column=" + sortCol                    +
            "&sort_dir="    + sortDir                    +
            "&query="       + urlEncode(query);

        std::string raw = fetchURL(url);
        if (raw.empty() || (raw.front() != '{' && raw.front() != '[')) continue;

        try {
            auto data = json::parse(raw);
            if (!data.contains("results") || !data["results"].is_array()) continue;

            for (auto& item : data["results"]) {
                if (!item.contains("hash_name") || !item.contains("sell_price") ||
                    !item.contains("sell_listings") || !item.contains("name") ||
                    !item.contains("sell_price_text") || !item.contains("sale_price_text") ||
                    !item.contains("asset_description"))
                    continue;

                std::string hash = item["hash_name"].get<std::string>();
                if (seen.count(hash)) continue;

                int price = item["sell_price"].get<int>();
                if (price < min_cents || price > max_cents) continue;

                auto& desc = item["asset_description"];
                if (!desc.contains("icon_url")) continue;

                seen.insert(hash);

                crow::json::wvalue skin;
                skin["name"]            = item["name"].get<std::string>();
                skin["hash_name"]       = hash;
                skin["sell_listings"]   = item["sell_listings"].get<int>();
                skin["sell_price"]      = price;
                skin["sell_price_text"] = item["sell_price_text"].get<std::string>();
                skin["sale_price_text"] = item["sale_price_text"].get<std::string>();
                skin["icon_url"]        = "https://community.akamai.steamstatic.com/economy/image/"
                                          + desc["icon_url"].get<std::string>();
                skin["market_url"]      = "https://steamcommunity.com/market/listings/730/"
                                          + urlEncode(hash);
                results.push_back(std::move(skin));
            }
        } catch (...) {
            continue;
        }
    }
}

// ─── Main ──────────────────────────────────────────────────

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
        int min_cents = (int)(min_d * 100);
        int max_cents = (int)(max_d * 100);

        std::vector<crow::json::wvalue> results;
        std::set<std::string>           seen;

        fetchAndMerge(query, "popular", "desc", 10, min_cents, max_cents, results, seen);
        fetchAndMerge(query, "price",   "desc", 10, min_cents, max_cents, results, seen);

        crow::json::wvalue r;
        r["total_count"] = (int)results.size();
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
            auto   body   = json::parse(req.body);
            double budget = body.value("budget", 0.0);
            std::string query = body.value("query", "");

            if (budget <= 0 || query.empty()) {
                crow::json::wvalue e;
                e["error"] = "Missing budget or query";
                return e;
            }

            int budget_cents = (int)(budget * 100);

            std::vector<Skin>     skins;
            std::set<std::string> seen;
            fetchQuery(query, 10, 1, budget_cents, skins, seen);

            if (skins.empty()) {
                crow::json::wvalue e;
                e["error"] = "No skins found within budget.";
                return e;
            }

            std::sort(skins.begin(), skins.end(), [](const Skin& a, const Skin& b) {
                return a.price_cents > b.price_cents;
            });

            std::vector<crow::json::wvalue> selected;
            for (auto& s : skins) {
                crow::json::wvalue sk;
                sk["name"]       = s.name;
                sk["price"]      = s.price_text;
                sk["listings"]   = s.listings;
                sk["icon_url"]   = s.icon_url;
                sk["market_url"] = s.market_url;
                selected.push_back(std::move(sk));
            }

            crow::json::wvalue r;
            r["budget"]      = budget;
            r["total_spent"] = 0.0;
            r["remaining"]   = budget;
            r["skins"]       = std::move(selected);
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

            if (weapons_budget <= 0) {
                crow::json::wvalue e;
                e["error"] = "weapons_budget must be greater than 0";
                return e;
            }

            // Split weapons budget evenly between primary and secondary
            int primary_cents   = (int)((weapons_budget / 2.0) * 100);
            int secondary_cents = (int)((weapons_budget / 2.0) * 100);
            int knife_cents     = (int)(knife_budget  * 100);
            int gloves_cents    = (int)(gloves_budget * 100);

            std::cerr << "[loadout/build] side=" << side
                      << " weapons=" << weapons_budget
                      << " knife="   << knife_budget
                      << " gloves="  << gloves_budget  << std::endl;

            // Weapon lists per side — individual queries, no OR syntax
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

            // Primary
            auto primary_opts = fetchSlotOptions(primary_queries, primary_cents, 5);
            if (!primary_opts.empty())
                slots["primary"] = std::move(primary_opts);
            else
                std::cerr << "[loadout/build] No primary options found" << std::endl;

            // Secondary
            auto secondary_opts = fetchSlotOptions(secondary_queries, secondary_cents, 5);
            if (!secondary_opts.empty())
                slots["secondary"] = std::move(secondary_opts);
            else
                std::cerr << "[loadout/build] No secondary options found" << std::endl;

            // Knife (optional)
            if (knife_cents > 0) {
                auto knife_opts = fetchSlotOptions({"Knife"}, knife_cents, 5);
                if (!knife_opts.empty())
                    slots["knife"] = std::move(knife_opts);
            }

            // Gloves (optional)
            if (gloves_cents > 0) {
                auto gloves_opts = fetchSlotOptions({"Gloves"}, gloves_cents, 5);
                if (!gloves_opts.empty())
                    slots["gloves"] = std::move(gloves_opts);
            }

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