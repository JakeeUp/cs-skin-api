#include "skinstrack.hpp"
#include <nlohmann/json.hpp>
#include <climits>
#include <cmath>
#include <cstdio>

using json = nlohmann::json;

// Anything above this is treated as garbage rather than a real price.
static constexpr double MAX_PRICE_USD = 1000000.0;

// ─── Parsing ───────────────────────────────────────────────

// Reads a numeric field as a non-negative int; missing/garbage -> 0.
static int intField(const json& obj, const char* key) {
    auto it = obj.find(key);
    if (it == obj.end() || !it->is_number()) return 0;
    double v = it->get<double>();
    if (!std::isfinite(v) || v <= 0) return 0;
    if (v >= static_cast<double>(INT_MAX)) return INT_MAX;
    return static_cast<int>(std::lround(v));
}

static std::string stringField(const json& obj, const char* key) {
    auto it = obj.find(key);
    return (it != obj.end() && it->is_string()) ? it->get<std::string>() : "";
}

// Epoch seconds field; missing, negative, or non-integer -> 0.
static long long epochField(const json& obj, const char* key) {
    auto it = obj.find(key);
    if (it == obj.end() || !it->is_number_integer()) return 0;
    long long v = it->get<long long>();
    return v > 0 ? v : 0;
}

// Price in cents of one provider entry, or -1 if unusable.
static int entryCents(const json& entry) {
    if (!entry.is_object()) return -1;
    auto it = entry.find("price");
    if (it == entry.end() || !it->is_number()) return -1;
    double usd = it->get<double>();
    if (!std::isfinite(usd) || usd <= 0 || usd > MAX_PRICE_USD) return -1;
    return static_cast<int>(std::lround(usd * 100.0));
}

static void parseItems(const json& items, SkinstrackPriceMap& out) {
    if (!items.is_array()) return;
    for (const auto& item : items) {
        if (!item.is_object()) continue;
        std::string name = stringField(item, "market_hash_name");
        if (name.empty()) continue;

        auto prices = item.find("prices");
        if (prices == item.end() || !prices->is_array()) continue;

        // Prefer the Steam entry; otherwise the first usable one.
        const json* chosen = nullptr;
        for (const auto& entry : *prices) {
            if (entryCents(entry) < 0) continue;
            if (stringField(entry, "provider") == "steam") { chosen = &entry; break; }
            if (!chosen) chosen = &entry;
        }
        if (!chosen) continue;

        SkinstrackPrice p;
        p.price_cents = entryCents(*chosen);
        p.liquidity   = intField(item, "liquidity");
        p.count       = intField(*chosen, "count");
        p.volume      = intField(*chosen, "volume");
        p.updated_at  = stringField(*chosen, "updated_at");
        p.icon_url    = stringField(item, "icon_url");
        out[name]     = std::move(p);
    }
}

SkinstrackSnapshot parseSkinstrackSnapshot(const std::string& body) {
    SkinstrackSnapshot snap;
    auto root = json::parse(body, nullptr, false);
    if (root.is_discarded()) return snap;

    if (root.is_array()) {
        parseItems(root, snap.prices);
    } else if (root.is_object()) {
        if (auto it = root.find("items"); it != root.end())
            parseItems(*it, snap.prices);
        snap.fetched_at = epochField(root, "fetched_at");
        snap.retry_at   = epochField(root, "retry_at");
    }
    return snap;
}

SkinstrackPriceMap parseSkinstrackItems(const std::string& body) {
    return parseSkinstrackSnapshot(body).prices;
}

// ─── Serialization ─────────────────────────────────────────

std::string serializeSkinstrackSnapshot(const SkinstrackSnapshot& snap) {
    json items = json::array();
    for (const auto& [name, p] : snap.prices) {
        json entry = {
            {"provider",   "steam"},
            {"price",      p.price_cents / 100.0},
            {"count",      p.count},
            {"volume",     p.volume},
            {"updated_at", p.updated_at},
        };
        items.push_back({
            {"market_hash_name", name},
            {"icon_url",         p.icon_url},
            {"liquidity",        p.liquidity},
            {"prices",           json::array({entry})},
        });
    }

    json root = {
        {"currency",   "USD"},
        {"fetched_at", snap.fetched_at},
        {"retry_at",   snap.retry_at},
        {"items",      std::move(items)},
    };
    // Item names are arbitrary UTF-8 from a third party; never throw on them.
    return root.dump(-1, ' ', false, json::error_handler_t::replace);
}

// ─── Time formatting ───────────────────────────────────────

std::string formatIsoUtc(long long t) {
    if (t < 0) t = 0;
    long long days = t / 86400;
    long long secs = t % 86400;

    // Civil-from-days (Howard Hinnant), valid for the proleptic Gregorian calendar.
    days += 719468;
    long long era = days / 146097;
    long long doe = days - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y   = yoe + era * 400;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp  = (5 * doy + 2) / 153;
    long long d   = doy - (153 * mp + 2) / 5 + 1;
    long long m   = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) y++;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04lld-%02lld-%02lldT%02lld:%02lld:%02lldZ",
                  y, m, d, secs / 3600, (secs / 60) % 60, secs % 60);
    return buf;
}
