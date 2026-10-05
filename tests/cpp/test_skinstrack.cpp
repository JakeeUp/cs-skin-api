#include "skinstrack.hpp"
#include "steam_market.hpp"
#include "test_harness.hpp"
#include <fstream>
#include <sstream>
#include <string>

static std::string readFixture(const std::string& name) {
    std::ifstream in(std::string(FIXTURE_DIR) + "/" + name, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// ─── parseSkinstrackItems ──────────────────────────────────

TEST(parse_prefers_steam_provider) {
    auto prices = parseSkinstrackItems(readFixture("skinstrack_items.json"));
    auto it = prices.find("AK-47 | Redline (Field-Tested)");
    CHECK(it != prices.end());
    CHECK_EQ(it->second.price_cents, 1075);
    CHECK_EQ(it->second.count, 150);
    CHECK_EQ(it->second.volume, 500);
    CHECK_EQ(it->second.liquidity, 85);
}

TEST(parse_expands_bare_icon_hash) {
    auto prices = parseSkinstrackItems(
        R"({"items":[{"market_hash_name":"X","icon_url":"abc123","prices":[{"price":1,"provider":"steam"}]}]})");
    CHECK_EQ(prices["X"].icon_url,
             std::string("https://community.akamai.steamstatic.com/economy/image/abc123"));

    auto full = parseSkinstrackItems(readFixture("skinstrack_items.json"));
    CHECK_EQ(full["AK-47 | Redline (Field-Tested)"].icon_url,
             std::string("https://community.akamai.steamstatic.com/economy/image/redline"));
}

TEST(parse_skips_items_without_usable_price) {
    auto prices = parseSkinstrackItems(readFixture("skinstrack_items.json"));
    CHECK_EQ(prices.size(), static_cast<size_t>(4));
    CHECK(prices.find("No Prices Case") == prices.end());
    CHECK(prices.find("Garbage Prices") == prices.end());
}

TEST(parse_accepts_bare_array) {
    auto prices = parseSkinstrackItems(readFixture("skinstrack_items_array.json"));
    CHECK(prices.find("M4A1-S | Printstream (Minimal Wear)") != prices.end());
}

TEST(parse_garbage_yields_empty_map) {
    CHECK(parseSkinstrackItems("").empty());
    CHECK(parseSkinstrackItems("<html>502</html>").empty());
    CHECK(parseSkinstrackItems("{\"items\": 5}").empty());
    CHECK(parseSkinstrackItems("[1, \"x\", null]").empty());
}

// ─── Snapshot round trip ───────────────────────────────────

TEST(snapshot_round_trips) {
    SkinstrackSnapshot snap;
    snap.prices     = parseSkinstrackItems(readFixture("skinstrack_items.json"));
    snap.fetched_at = 1790000000;
    snap.retry_at   = 1790003600;

    auto back = parseSkinstrackSnapshot(serializeSkinstrackSnapshot(snap));
    CHECK_EQ(back.fetched_at, snap.fetched_at);
    CHECK_EQ(back.retry_at,   snap.retry_at);
    CHECK_EQ(back.prices.size(), snap.prices.size());
    CHECK_EQ(back.prices["AK-47 | Redline (Field-Tested)"].price_cents, 1075);
}

TEST(format_iso_utc) {
    CHECK_EQ(formatIsoUtc(0),         std::string("1970-01-01T00:00:00Z"));
    CHECK_EQ(formatIsoUtc(951782400), std::string("2000-02-29T00:00:00Z"));
}

// ─── rankTrending ──────────────────────────────────────────

static SkinstrackPrice priced(int cents, int liquidity, int count) {
    SkinstrackPrice p;
    p.price_cents = cents;
    p.liquidity   = liquidity;
    p.count       = count;
    return p;
}

TEST(trending_ranks_by_liquidity_then_offers) {
    SkinstrackPriceMap m;
    m["AK-47 | Slate (Field-Tested)"]     = priced(500, 90, 100);
    m["AWP | Asiimov (Field-Tested)"]     = priced(9000, 95, 10);
    m["M4A4 | Howl (Minimal Wear)"]       = priced(9000, 90, 500);
    auto t = rankTrending(m, 10, 100);
    CHECK_EQ(t.size(), static_cast<size_t>(3));
    CHECK_EQ(t[0].name, std::string("AWP | Asiimov (Field-Tested)"));
    CHECK_EQ(t[1].name, std::string("M4A4 | Howl (Minimal Wear)"));
}

TEST(trending_excludes_non_skins_and_cheap_items) {
    SkinstrackPriceMap m;
    m["Sticker | Crown (Foil)"]           = priced(50000, 100, 999);
    m["Charm | Lil' Ava"]                 = priced(500, 100, 999);
    m["Revolution Case"]                  = priced(500, 100, 999);
    m["P250 | Sand Dune (Field-Tested)"]  = priced(5, 100, 999);
    m["AK-47 | Redline (Field-Tested)"]   = priced(3488, 85, 1176);
    auto t = rankTrending(m, 10, 100);
    CHECK_EQ(t.size(), static_cast<size_t>(1));
    CHECK_EQ(t[0].name, std::string("AK-47 | Redline (Field-Tested)"));
}

TEST(trending_keeps_one_variant_per_skin) {
    SkinstrackPriceMap m;
    m["AK-47 | Redline (Field-Tested)"]            = priced(3488, 85, 1176);
    m["StatTrak™ AK-47 | Redline (Minimal Wear)"]  = priced(9000, 80, 50);
    m["★ Karambit | Fade (Factory New)"]           = priced(200000, 70, 20);
    m["★ StatTrak™ Karambit | Fade (Factory New)"] = priced(300000, 60, 5);
    auto t = rankTrending(m, 10, 100);
    CHECK_EQ(t.size(), static_cast<size_t>(2));
    CHECK_EQ(t[0].name, std::string("AK-47 | Redline (Field-Tested)"));
    CHECK_EQ(t[1].name, std::string("★ Karambit | Fade (Factory New)"));
}

TEST(trending_respects_limit) {
    SkinstrackPriceMap m;
    m["AK-47 | Slate (Field-Tested)"]  = priced(500, 90, 1);
    m["AWP | Asiimov (Field-Tested)"]  = priced(500, 90, 2);
    m["M4A4 | Howl (Minimal Wear)"]    = priced(500, 90, 3);
    CHECK_EQ(rankTrending(m, 2, 100).size(), static_cast<size_t>(2));
    CHECK(rankTrending(SkinstrackPriceMap{}, 5, 0).empty());
}

// ─── extractRarity ─────────────────────────────────────────

TEST(rarity_from_steam_type) {
    CHECK_EQ(extractRarity("Covert Rifle"),                std::string("Covert"));
    CHECK_EQ(extractRarity("StatTrak™ Classified Pistol"), std::string("Classified"));
    CHECK_EQ(extractRarity("★ StatTrak™ Covert Knife"),    std::string("Covert"));
    CHECK_EQ(extractRarity("Mil-Spec Grade SMG"),          std::string("Mil-Spec Grade"));
    CHECK_EQ(extractRarity("Extraordinary Gloves"),        std::string("Extraordinary"));
    CHECK_EQ(extractRarity("Souvenir Restricted Rifle"),   std::string("Restricted"));
}

TEST(rarity_unknown_is_empty) {
    CHECK_EQ(extractRarity(""),                     std::string(""));
    CHECK_EQ(extractRarity("Base Grade Container"), std::string(""));
    CHECK_EQ(extractRarity("Covertness"),           std::string(""));
}

int main() { return runTests(); }
