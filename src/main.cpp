#include "crow_all.h"
#include "config.hpp"
#include "http_client.hpp"
#include "optimizer.hpp"
#include "skinstrack.hpp"
#include "steam_market.hpp"
#include "validation.hpp"
#include <nlohmann/json.hpp>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using json = nlohmann::json;

// POST bodies are tiny JSON objects; anything larger is rejected.
static constexpr size_t MAX_BODY_BYTES = 4096;

// ─── Response helpers ──────────────────────────────────────

static crow::response jsonResponse(int code, const crow::json::wvalue& body) {
    crow::response res(code, body.dump());
    res.set_header("Content-Type",           "application/json");
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("Cache-Control",          "no-store");
    return res;
}

static crow::response jsonError(int code, const std::string& message) {
    crow::json::wvalue e;
    e["error"] = message;
    return jsonResponse(code, e);
}

// Parses a POST body as a JSON object, or explains why it can't.
static std::optional<json> parseBody(const crow::request& req, crow::response& error) {
    if (req.body.size() > MAX_BODY_BYTES) {
        error = jsonError(413, "Request body too large");
        return std::nullopt;
    }
    auto body = json::parse(req.body, nullptr, false);
    if (body.is_discarded() || !body.is_object()) {
        error = jsonError(400, "Body must be a JSON object");
        return std::nullopt;
    }
    return body;
}

// Reads an optional numeric dollar field from a JSON body.
static std::optional<double> bodyDollars(const json& body, const char* key) {
    if (!body.contains(key)) return 0.0;
    if (!body[key].is_number()) return std::nullopt;
    return checkDollars(body[key].get<double>());
}

// Reads an optional string field; a wrong type yields "" so validation rejects it.
static std::string bodyString(const json& body, const char* key, const std::string& fallback) {
    if (!body.contains(key)) return fallback;
    return body[key].is_string() ? body[key].get<std::string>() : "";
}

// ─── Skin serialization ────────────────────────────────────

static crow::json::wvalue skinstrackJson(const SkinstrackPrice& p) {
    crow::json::wvalue j;
    j["price_cents"] = p.price_cents;
    j["liquidity"]   = p.liquidity;
    j["count"]       = p.count;
    j["volume"]      = p.volume;
    j["updated_at"]  = p.updated_at;
    return j;
}

// Adds "skinstrack" when the store has a price for this item.
static void addSkinstrack(crow::json::wvalue& j, const Skin& s, const SkinstrackStore& store) {
    if (auto p = store.find(s.hash_name))
        j["skinstrack"] = skinstrackJson(*p);
}

// Full Steam search response shape (used by /search).
static crow::json::wvalue skinToJson(const Skin& s, const SkinstrackStore& store) {
    crow::json::wvalue j;
    j["name"]            = s.name;
    j["hash_name"]       = s.hash_name;
    j["sell_price"]      = s.price_cents;
    j["sell_price_text"] = s.price_text;
    j["sale_price_text"] = s.sale_price_text;
    j["sell_listings"]   = s.listings;
    j["icon_url"]        = s.icon_url;
    j["market_url"]      = s.market_url;
    j["rarity"]          = s.rarity;
    addSkinstrack(j, s, store);
    return j;
}

// Compact shape used by /budget/optimize and /loadout/build.
static crow::json::wvalue skinOptionJson(const Skin& s, const SkinstrackStore& store) {
    crow::json::wvalue o;
    o["name"]        = s.name;
    o["price"]       = s.price_text;
    o["price_cents"] = s.price_cents;
    o["listings"]    = s.listings;
    o["icon_url"]    = s.icon_url;
    o["market_url"]  = s.market_url;
    o["rarity"]      = s.rarity;
    addSkinstrack(o, s, store);
    return o;
}

static std::vector<crow::json::wvalue> toOptionList(const std::vector<Skin>&  skins,
                                                    const SkinstrackStore&    store) {
    std::vector<crow::json::wvalue> out;
    out.reserve(skins.size());
    for (const auto& s : skins) out.push_back(skinOptionJson(s, store));
    return out;
}

// ─── Main ──────────────────────────────────────────────────

int main() {
    const Config config = loadConfig();
    setHttpCaBundle(config.ca_bundle);

    SkinstrackStore skinstrack(config);
    skinstrack.start();

    crow::App<crow::CORSHandler> app;
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .headers("Content-Type")
        .methods("GET"_method, "POST"_method);
    if (!config.allowed_origin.empty())
        cors.global().origin(config.allowed_origin);

    // GET /
    CROW_ROUTE(app, "/")([]() {
        return "CS Skin API is running!";
    });

    // GET /health
    CROW_ROUTE(app, "/health")([]() {
        crow::json::wvalue r;
        r["status"]  = "ok";
        r["message"] = "CS Skin API is alive";
        return jsonResponse(200, r);
    });

    // GET /search?q=AK-47&min=0&max=300
    CROW_ROUTE(app, "/search")([&skinstrack](const crow::request& req) {
        const char* q = req.url_params.get("q");
        std::string query = q ? q : "";
        if (!isValidQuery(query))
            return jsonError(400, "Query ?q= is required (max 64 characters)");

        int min_cents = 0;
        int max_cents = INT_MAX;
        if (const char* v = req.url_params.get("min")) {
            auto d = parseDollars(v);
            if (!d) return jsonError(400, "min must be a dollar amount between 0 and 10000");
            min_cents = toCents(*d);
        }
        if (const char* v = req.url_params.get("max")) {
            auto d = parseDollars(v);
            if (!d) return jsonError(400, "max must be a dollar amount between 0 and 10000");
            max_cents = toCents(*d);
        }
        if (min_cents > max_cents)
            return jsonError(400, "min cannot be greater than max");

        std::vector<Skin>     skins;
        std::set<std::string> seen;
        fetchQuery(query, 10, min_cents, max_cents, skins, seen);

        std::vector<crow::json::wvalue> results;
        results.reserve(skins.size());
        for (const auto& s : skins)
            results.push_back(skinToJson(s, skinstrack));

        crow::json::wvalue r;
        r["total_count"] = static_cast<int>(results.size());
        r["results"]     = std::move(results);
        return jsonResponse(200, r);
    });

    // GET /price?name=AK-47+Redline+(Field-Tested)
    CROW_ROUTE(app, "/price")([](const crow::request& req) {
        const char* n = req.url_params.get("name");
        std::string name = n ? n : "";
        if (name.empty() || name.size() > 128)
            return jsonError(400, "Missing or too long name parameter ?name=");

        std::string raw = fetchURL(
            "https://steamcommunity.com/market/priceoverview/?appid=730&currency=1"
            "&market_hash_name=" + urlEncode(name));

        auto data = json::parse(raw, nullptr, false);
        if (data.is_discarded() || !data.is_object())
            return jsonError(502, "Steam did not return a price for that item");

        crow::json::wvalue r;
        r["name"]         = name;
        r["lowest_price"] = data.value("lowest_price", "N/A");
        r["median_price"] = data.value("median_price", "N/A");
        r["volume"]       = data.value("volume", "N/A");
        return jsonResponse(200, r);
    });

    // GET /skinstrack/status
    CROW_ROUTE(app, "/skinstrack/status")([&skinstrack]() {
        SkinstrackStatus s = skinstrack.status();
        crow::json::wvalue r;
        r["configured"]    = s.configured;
        r["items"]         = s.items;
        r["refresh_hours"] = s.refresh_hours;
        if (s.fetched_at > 0) r["fetched_at"] = formatIsoUtc(s.fetched_at);
        else                  r["fetched_at"] = nullptr;
        if (!s.last_error.empty()) r["last_error"] = s.last_error;
        else                       r["last_error"] = nullptr;
        return jsonResponse(200, r);
    });

    // GET /skinstrack/price?name=AK-47+|+Redline+(Field-Tested)
    CROW_ROUTE(app, "/skinstrack/price")([&skinstrack](const crow::request& req) {
        const char* n = req.url_params.get("name");
        std::string name = n ? n : "";
        if (name.empty() || name.size() > 128)
            return jsonError(400, "Missing or too long name parameter ?name=");

        if (skinstrack.status().items == 0)
            return jsonError(503, "SkinsTrack data not loaded");

        auto p = skinstrack.find(name);
        if (!p) return jsonError(404, "No SkinsTrack price for that item");

        crow::json::wvalue r;
        r["name"]        = name;
        r["price_cents"] = p->price_cents;
        r["liquidity"]   = p->liquidity;
        r["count"]       = p->count;
        r["volume"]      = p->volume;
        r["updated_at"]  = p->updated_at;
        r["icon_url"]    = p->icon_url;
        return jsonResponse(200, r);
    });

    // GET /skinstrack/trending?limit=24&min=1
    // Served from the cached snapshot, so it never spends SkinsTrack calls.
    CROW_ROUTE(app, "/skinstrack/trending")([&skinstrack](const crow::request& req) {
        int limit = 24;
        if (const char* v = req.url_params.get("limit")) {
            char* end = nullptr;
            long l = std::strtol(v, &end, 10);
            if (*v == '\0' || *end != '\0' || l < 1 || l > 100)
                return jsonError(400, "limit must be between 1 and 100");
            limit = static_cast<int>(l);
        }
        int min_cents = 100;
        if (const char* v = req.url_params.get("min")) {
            auto d = parseDollars(v);
            if (!d) return jsonError(400, "min must be a dollar amount between 0 and 10000");
            min_cents = toCents(*d);
        }

        if (skinstrack.status().items == 0)
            return jsonError(503, "SkinsTrack data not loaded");

        std::vector<crow::json::wvalue> items;
        for (const auto& t : skinstrack.trending(static_cast<size_t>(limit), min_cents)) {
            crow::json::wvalue o;
            o["name"]        = t.name;
            o["price_cents"] = t.price.price_cents;
            o["icon_url"]    = t.price.icon_url;
            o["market_url"]  = "https://steamcommunity.com/market/listings/730/" + urlEncode(t.name);
            o["skinstrack"]["price_cents"] = t.price.price_cents;
            o["skinstrack"]["liquidity"]   = t.price.liquidity;
            o["skinstrack"]["count"]       = t.price.count;
            o["skinstrack"]["volume"]      = t.price.volume;
            o["skinstrack"]["updated_at"]  = t.price.updated_at;
            items.push_back(std::move(o));
        }

        crow::json::wvalue r;
        r["count"] = static_cast<int>(items.size());
        r["items"] = std::move(items);
        return jsonResponse(200, r);
    });

    // POST /budget/optimize
    // Body: { "budget": 50.00, "query": "AK-47" }
    CROW_ROUTE(app, "/budget/optimize").methods(crow::HTTPMethod::Post)([&skinstrack](const crow::request& req) {
        crow::response error;
        auto body = parseBody(req, error);
        if (!body) return error;

        auto budget = bodyDollars(*body, "budget");
        std::string query = bodyString(*body, "query", "");

        if (!budget || *budget <= 0)
            return jsonError(400, "budget must be between 0 and 10000");
        if (!isValidQuery(query))
            return jsonError(400, "query is required (max 64 characters)");

        int budget_cents = toCents(*budget);

        std::vector<Skin>     skins;
        std::set<std::string> seen;
        fetchQuery(query, 10, 1, budget_cents, skins, seen);

        if (skins.empty())
            return jsonError(404, "No skins found within budget.");

        // Run knapsack to find the optimal combination within budget
        auto selected = knapsackOptimize(skins, budget_cents);

        int total_cents = 0;
        for (const auto& s : selected) total_cents += s.price_cents;

        crow::json::wvalue r;
        r["budget"]         = *budget;
        r["total_spent"]    = total_cents / 100.0;
        r["remaining"]      = (budget_cents - total_cents) / 100.0;
        r["skins_found"]    = static_cast<int>(skins.size());
        r["skins_selected"] = static_cast<int>(selected.size());
        r["algorithm"]      = usesGreedy(budget_cents, static_cast<int>(skins.size()))
                               ? "greedy" : "knapsack_dp";
        r["skins"]          = toOptionList(selected, skinstrack);
        return jsonResponse(200, r);
    });

    // POST /loadout/build
    // Body:
    // {
    //   "side":           "T" | "CT",
    //   "weapons_budget": 100.00,   -- split evenly across primary + secondary
    //   "knife_budget":    50.00,   -- 0 or omitted = skip
    //   "gloves_budget":   30.00    -- 0 or omitted = skip
    // }
    CROW_ROUTE(app, "/loadout/build").methods(crow::HTTPMethod::Post)([&skinstrack](const crow::request& req) {
        crow::response error;
        auto body = parseBody(req, error);
        if (!body) return error;

        std::string side = bodyString(*body, "side", "T");
        auto weapons_budget = bodyDollars(*body, "weapons_budget");
        auto knife_budget   = bodyDollars(*body, "knife_budget");
        auto gloves_budget  = bodyDollars(*body, "gloves_budget");

        if (side != "T" && side != "CT")
            return jsonError(400, "side must be 'T' or 'CT'");
        if (!weapons_budget || *weapons_budget <= 0)
            return jsonError(400, "weapons_budget must be between 0 and 10000");
        if (!knife_budget || !gloves_budget)
            return jsonError(400, "knife_budget and gloves_budget must be between 0 and 10000");

        int per_weapon_cents = toCents(*weapons_budget / 2.0);
        int knife_cents      = toCents(*knife_budget);
        int gloves_cents     = toCents(*gloves_budget);

        std::cerr << "[loadout/build] side=" << side
                  << " weapons=" << *weapons_budget
                  << " knife="   << *knife_budget
                  << " gloves="  << *gloves_budget << std::endl;

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
            if (!opts.empty()) slots[key] = toOptionList(opts, skinstrack);
        };

        addSlot("primary",   primary_queries,   per_weapon_cents);
        addSlot("secondary", secondary_queries, per_weapon_cents);
        if (knife_cents  > 0) addSlot("knife",  {"Knife"},  knife_cents);
        if (gloves_cents > 0) addSlot("gloves", {"Gloves"}, gloves_cents);

        crow::json::wvalue r;
        r["side"]           = side;
        r["weapons_budget"] = *weapons_budget;
        r["knife_budget"]   = *knife_budget;
        r["gloves_budget"]  = *gloves_budget;
        r["slots"]          = std::move(slots);
        return jsonResponse(200, r);
    });

    app.port(static_cast<uint16_t>(config.port)).multithreaded().run();
}
