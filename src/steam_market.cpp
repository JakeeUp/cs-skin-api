#include "steam_market.hpp"
#include "http_client.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

using json = nlohmann::json;

// ─── Rarity ────────────────────────────────────────────────

std::string extractRarity(const std::string& type) {
    static const char* const PREFIXES[] = {
        "\xE2\x98\x85 ",          // "★ "
        "StatTrak\xE2\x84\xA2 ",  // "StatTrak™ "
        "Souvenir ",
    };
    static const char* const RARITIES[] = {
        "Consumer Grade", "Industrial Grade", "Mil-Spec Grade", "Restricted",
        "Classified",     "Covert",           "Contraband",     "Extraordinary",
    };

    // Strip any combination of the star / StatTrak / Souvenir prefixes.
    std::string t = type;
    for (bool stripped = true; stripped; ) {
        stripped = false;
        for (const char* p : PREFIXES) {
            std::string prefix = p;
            if (t.compare(0, prefix.size(), prefix) == 0) {
                t.erase(0, prefix.size());
                stripped = true;
            }
        }
    }

    // The rarity must be a whole leading word group: "Covert Knife", "Covert".
    for (const char* r : RARITIES) {
        std::string rarity = r;
        if (t.compare(0, rarity.size(), rarity) == 0 &&
            (t.size() == rarity.size() || t[rarity.size()] == ' '))
            return rarity;
    }
    return "";
}

// ─── Steam search ──────────────────────────────────────────

// Rate-limit delay between Steam API requests to avoid HTTP 429
static constexpr int STEAM_RATE_LIMIT_MS = 150;

// Fetches one page of Steam market results for a query.
// Appends valid skins (price within range) into `skins`, deduplicating via `seen`.
static void fetchPage(
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

            std::string type;
            if (desc.contains("type") && desc["type"].is_string())
                type = desc["type"].get<std::string>();

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
                listings,
                extractRarity(type)
            });
            added++;
        }

        std::cerr << "[fetchPage] Added " << added << " skins for: " << query << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[fetchPage] JSON parse error: " << e.what()
                  << " | query: " << query << std::endl;
    }
}

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

std::vector<Skin> fetchSlotOptions(
    const std::vector<std::string>& queries,
    int                             budget_cents,
    int                             max_options
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

    std::cerr << "[fetchSlotOptions] Returning " << interleaved.size()
              << " options across " << perWeapon.size()
              << " weapons (budget=" << budget_cents << "c)" << std::endl;

    return interleaved;
}
