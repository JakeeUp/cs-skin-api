#include "crow_all.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <thread>
#include <chrono>

using json = nlohmann::json;

// ─── CURL Helpers ──────────────────────────────────────────

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    output->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    std::string encoded;
    if (curl) {
        char* out = curl_easy_escape(curl, str.c_str(), static_cast<int>(str.length()));
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

    // SSL verification disabled: MSYS2/MinGW lacks a system CA bundle, causing
    // certificate validation failures against Steam's CDN. In a production
    // deployment, set CURLOPT_CAINFO to a valid CA bundle path instead.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    // Steam requires browser-like headers to serve JSON
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
    std::string hash_name;
    std::string price_text;
    std::string sale_price_text;
    std::string icon_url;
    std::string market_url;
    int         price_cents;
    int         listings;
};

// ─── Core Steam Market Fetch ───────────────────────────────

// Rate-limit delay between Steam API requests to avoid HTTP 429
static constexpr int STEAM_RATE_LIMIT_MS = 150;

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
        "&start="       + std::to_string(start) +
        "&sort_column=" + sortCol               +
        "&sort_dir="    + sortDir               +
        "&query="       + urlEncode(query);

    std::cerr << "[fetchPage] " << query << " | start=" << start
              << " | sort=" << sortCol << std::endl;

    std::string raw = fetchURL(url);

    if (raw.empty()) {
        std::cerr << "[fetchPage] Empty response for: " << query << std::endl;
        return;
    }

    // Steam sometimes returns HTML error pages instead of JSON
    if (raw.front() != '{' && raw.front() != '[') {
        std::cerr << "[fetchPage] Non-JSON response (" << raw.length()
                  << " bytes) for: " << query << std::endl;
        return;
    }

    try {
        auto data = json::parse(raw);

        if (!data.contains("results") || !data["results"].is_array()) {
            std::cerr << "[fetchPage] No results array for: " << query << std::endl;
            return;
        }

        int added = 0;
        for (auto& item : data["results"]) {
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
                hash,
                item["sell_price_text"].get<std::string>(),
                item.value("sale_price_text", ""),
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

// Fetches multiple pages for a query across two sort orders (popular + price).
// Inserts rate-limit delays between Steam API calls to avoid throttling.
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
        std::this_thread::sleep_for(std::chrono::milliseconds(STEAM_RATE_LIMIT_MS));
        fetchPage(query, "price",   "desc", p * 10, min_cents, max_cents, skins, seen);
        if (p < pages - 1)
            std::this_thread::sleep_for(std::chrono::milliseconds(STEAM_RATE_LIMIT_MS));
    }
}

// Converts a Skin struct to a crow JSON value for API responses.
crow::json::wvalue skinToJson(const Skin& s) {
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

// ─── Loadout Slot Fetcher ──────────────────────────────────

// Fetches options per weapon query, takes the best result from each weapon,
// then fills remaining slots round-robin with next-best across all weapons.
// This ensures variety — e.g. one AK-47, one SG 553, one Galil AR — rather than
// all slots going to whichever weapon has the most cheap listings.
std::vector<crow::json::wvalue> fetchSlotOptions(
    const std::vector<std::string>& queries,
    int                             budget_cents,
    int                             max_options = 5
) {
    std::vector<std::vector<Skin>> perWeapon;
    std::set<std::string> globalSeen;

    for (const auto& q : queries) {
        std::vector<Skin>     weaponSkins;
        std::set<std::string> weaponSeen;
        fetchQuery(q, 3, 1, budget_cents, weaponSkins, weaponSeen);

        std::sort(weaponSkins.begin(), weaponSkins.end(), [](const Skin& a, const Skin& b) {
            return a.price_cents > b.price_cents;
        });

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

    // Interleave: pick best from each weapon round-robin, then second-best, etc.
    std::vector<Skin> interleaved;
    size_t maxDepth = 0;
    for (auto& w : perWeapon)
        if (w.size() > maxDepth) maxDepth = w.size();

    for (size_t depth = 0; depth < maxDepth && static_cast<int>(interleaved.size()) < max_options; depth++) {
        for (auto& w : perWeapon) {
            if (static_cast<int>(interleaved.size()) >= max_options) break;
            if (depth < w.size())
                interleaved.push_back(w[depth]);
        }
    }

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

// ─── 0/1 Knapsack Budget Optimizer ─────────────────────────
//
// Selects the combination of skins that maximizes total value spent
// without exceeding the budget — a classic 0/1 knapsack problem.
//
// Uses dynamic programming for budgets <= $500 with <= 150 items
// (DP table fits in ~15MB). Falls back to a greedy heuristic
// (largest-first) for larger inputs where DP would be impractical.

std::vector<Skin> knapsackOptimize(const std::vector<Skin>& items, int capacity) {
    int n = static_cast<int>(items.size());

    // Greedy fallback: sort by price descending, greedily pick items that fit.
    // Optimal for the "maximize spending" objective when DP is too expensive.
    if (capacity > 50000 || n > 150) {
        auto sorted = items;
        std::sort(sorted.begin(), sorted.end(), [](const Skin& a, const Skin& b) {
            return a.price_cents > b.price_cents;
        });
        std::vector<Skin> result;
        int remaining = capacity;
        for (auto& s : sorted) {
            if (s.price_cents <= remaining) {
                result.push_back(s);
                remaining -= s.price_cents;
            }
        }
        return result;
    }

    // dp[w] = maximum total price achievable with capacity w
    std::vector<int> dp(capacity + 1, 0);
    // keep[i][w] = whether item i was selected at capacity w
    std::vector<std::vector<bool>> keep(n, std::vector<bool>(capacity + 1, false));

    for (int i = 0; i < n; i++) {
        int cost = items[i].price_cents;
        // Iterate capacity in reverse to prevent using the same item twice
        for (int w = capacity; w >= cost; w--) {
            if (dp[w - cost] + cost > dp[w]) {
                dp[w] = dp[w - cost] + cost;
                keep[i][w] = true;
            }
        }
    }

    // Backtrack through the keep table to recover the selected set
    std::vector<Skin> result;
    int w = capacity;
    for (int i = n - 1; i >= 0; i--) {
        if (keep[i][w]) {
            result.push_back(items[i]);
            w -= items[i].price_cents;
        }
    }

    return result;
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
            double budget   = body.value("budget", 0.0);
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
            std::vector<crow::json::wvalue> selectedJson;
            for (auto& s : selected) {
                total_cents += s.price_cents;
                crow::json::wvalue sk;
                sk["name"]        = s.name;
                sk["price"]       = s.price_text;
                sk["price_cents"] = s.price_cents;
                sk["listings"]    = s.listings;
                sk["icon_url"]    = s.icon_url;
                sk["market_url"]  = s.market_url;
                selectedJson.push_back(std::move(sk));
            }

            double total_spent = total_cents / 100.0;

            crow::json::wvalue r;
            r["budget"]         = budget;
            r["total_spent"]    = total_spent;
            r["remaining"]      = budget - total_spent;
            r["skins_found"]    = static_cast<int>(skins.size());
            r["skins_selected"] = static_cast<int>(selected.size());
            r["algorithm"]      = (budget_cents > 50000 || static_cast<int>(skins.size()) > 150)
                                   ? "greedy" : "knapsack_dp";
            r["skins"]          = std::move(selectedJson);
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

            int primary_cents   = static_cast<int>((weapons_budget / 2.0) * 100);
            int secondary_cents = static_cast<int>((weapons_budget / 2.0) * 100);
            int knife_cents     = static_cast<int>(knife_budget  * 100);
            int gloves_cents    = static_cast<int>(gloves_budget * 100);

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

            auto primary_opts = fetchSlotOptions(primary_queries, primary_cents, 5);
            if (!primary_opts.empty())
                slots["primary"] = std::move(primary_opts);

            auto secondary_opts = fetchSlotOptions(secondary_queries, secondary_cents, 5);
            if (!secondary_opts.empty())
                slots["secondary"] = std::move(secondary_opts);

            if (knife_cents > 0) {
                auto knife_opts = fetchSlotOptions({"Knife"}, knife_cents, 5);
                if (!knife_opts.empty())
                    slots["knife"] = std::move(knife_opts);
            }

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
