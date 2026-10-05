#include "skinstrack.hpp"
#include "http_client.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

// Anything above this is treated as garbage rather than a real price.
static constexpr double MAX_PRICE_USD = 1000000.0;

static const char* const SKINSTRACK_ITEMS_URL =
    "https://api.skinstrack.com/v2/free/items?currency=USD";

// The full item list is a large download; a timed-out attempt still costs a call.
static constexpr long SKINSTRACK_TIMEOUT_SECONDS = 120;

// First retry delay after a transient failure; doubles up to the refresh interval.
static constexpr long long SKINSTRACK_MIN_BACKOFF_SECONDS = 3600;

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

// ─── Price store ───────────────────────────────────────────

static long long nowEpoch() {
    return static_cast<long long>(std::time(nullptr));
}

SkinstrackStore::SkinstrackStore(const Config& config)
    : api_key_      (config.skinstrack_api_key),
      refresh_hours_(config.skinstrack_refresh_hours),
      cache_file_   (config.skinstrack_cache_file),
      prices_       (std::make_shared<const SkinstrackPriceMap>()) {}

SkinstrackStore::~SkinstrackStore() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void SkinstrackStore::start() {
    loadCache();
    if (api_key_.empty()) {
        std::cerr << "[skinstrack] No API key; serving cached data only" << std::endl;
        return;
    }
    thread_ = std::thread(&SkinstrackStore::run, this);
}

std::optional<SkinstrackPrice> SkinstrackStore::find(const std::string& name) const {
    std::shared_ptr<const SkinstrackPriceMap> prices;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        prices = prices_;
    }
    auto it = prices->find(name);
    if (it == prices->end()) return std::nullopt;
    return it->second;
}

SkinstrackStatus SkinstrackStore::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    SkinstrackStatus s;
    s.configured    = !api_key_.empty();
    s.items         = static_cast<int>(prices_->size());
    s.fetched_at    = prices_->empty() ? 0 : fetched_at_;
    s.refresh_hours = refresh_hours_;
    s.last_error    = last_error_;
    return s;
}

// ─── Disk cache ────────────────────────────────────────────

void SkinstrackStore::loadCache() {
    std::ifstream file(cache_file_, std::ios::binary);
    if (!file) {
        std::cerr << "[skinstrack] No cache at " << cache_file_ << std::endl;
        return;
    }
    std::stringstream buf;
    buf << file.rdbuf();
    SkinstrackSnapshot snap = parseSkinstrackSnapshot(buf.str());

    // A timestamp in the future (clock change, hand-edited file) must not
    // postpone refreshes indefinitely.
    long long now      = nowEpoch();
    long long interval = static_cast<long long>(refresh_hours_) * 3600;
    if (snap.fetched_at > now + 3600) snap.fetched_at = 0;
    if (snap.retry_at   > now + interval) snap.retry_at = now + interval;

    std::cerr << "[skinstrack] Loaded " << snap.prices.size() << " items from "
              << cache_file_ << " (fetched_at="
              << (snap.fetched_at ? formatIsoUtc(snap.fetched_at) : "unknown") << ")"
              << std::endl;

    std::lock_guard<std::mutex> lock(mutex_);
    prices_     = std::make_shared<const SkinstrackPriceMap>(std::move(snap.prices));
    fetched_at_ = snap.fetched_at;
    retry_at_   = snap.retry_at;
}

// Writes to a temp file and renames it over the cache, so a crash mid-write
// never leaves a truncated snapshot behind.
bool SkinstrackStore::saveCache(const SkinstrackSnapshot& snap) const {
    std::error_code ec;
    fs::path target = cache_file_;
    if (target.has_parent_path())
        fs::create_directories(target.parent_path(), ec);

    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << serializeSkinstrackSnapshot(snap);
        out.flush();
        if (!out) {
            std::cerr << "[skinstrack] Could not write " << tmp.string() << std::endl;
            fs::remove(tmp, ec);
            return false;
        }
    }
    fs::rename(tmp, target, ec);
    if (ec) {
        std::cerr << "[skinstrack] Could not replace " << cache_file_
                  << ": " << ec.message() << std::endl;
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

// ─── Refresh thread ────────────────────────────────────────

bool SkinstrackStore::sleepUntil(long long due) {
    std::unique_lock<std::mutex> lock(mutex_);
    // Wake at least hourly to re-check the wall clock (sleep/hibernate, clock changes).
    while (!stopping_ && nowEpoch() < due) {
        long long wait = std::min<long long>(due - nowEpoch(), 3600);
        wake_.wait_for(lock, std::chrono::seconds(std::max<long long>(wait, 1)));
    }
    return !stopping_;
}

// Records a failed attempt and persists the hold-off so a restart honors it.
void SkinstrackStore::holdOff(long long until, const std::string& error) {
    SkinstrackSnapshot snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        retry_at_   = until;
        last_error_ = error;
        snap.prices     = *prices_;
        snap.fetched_at = fetched_at_;
        snap.retry_at   = retry_at_;
    }
    std::cerr << "[skinstrack] " << error << "; next attempt at "
              << formatIsoUtc(until) << std::endl;
    saveCache(snap);
}

SkinstrackStore::FetchResult SkinstrackStore::fetchOnce(std::string& error) {
    std::cerr << "[skinstrack] Fetching item list" << std::endl;

    // The key goes only in a header: httpGet logs URLs on failure.
    // SkinsTrack's Cloudflare front answers 403 to the browser-style
    // User-Agent that Steam needs, so identify as a plain API client here.
    HttpResponse r = httpGet(SKINSTRACK_ITEMS_URL,
                             {"X-API-KEY: " + api_key_, "User-Agent: cs-skin-api/1.0"},
                             SKINSTRACK_TIMEOUT_SECONDS);

    switch (r.status) {
        case 200: break;
        case 401:
            error = "API key rejected (HTTP 401); fix SKINSTRACK_API_KEY and restart";
            return FetchResult::Unauthorized;
        case 429:
            error = "Monthly request limit reached (HTTP 429)";
            return FetchResult::HoldOff;
        case 400:
            error = "Request rejected (HTTP 400)";
            return FetchResult::HoldOff;
        case 0:
            error = "Network error";
            return FetchResult::Failed;
        default:
            error = "Unexpected HTTP " + std::to_string(r.status);
            return FetchResult::Failed;
    }

    SkinstrackSnapshot snap;
    snap.prices     = parseSkinstrackItems(r.body);
    snap.fetched_at = nowEpoch();
    if (snap.prices.empty()) {
        error = "Response contained no usable items";
        return FetchResult::Failed;
    }

    if (!saveCache(snap))
        std::cerr << "[skinstrack] Snapshot not persisted; a restart will refetch" << std::endl;

    std::cerr << "[skinstrack] Fetched " << snap.prices.size() << " items" << std::endl;

    std::lock_guard<std::mutex> lock(mutex_);
    prices_     = std::make_shared<const SkinstrackPriceMap>(std::move(snap.prices));
    fetched_at_ = snap.fetched_at;
    retry_at_   = 0;
    last_error_.clear();
    return FetchResult::Ok;
}

void SkinstrackStore::run() {
    const long long interval = static_cast<long long>(refresh_hours_) * 3600;
    long long backoff = std::min(SKINSTRACK_MIN_BACKOFF_SECONDS, interval);

    while (true) {
        long long due;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            due = std::max(fetched_at_ > 0 ? fetched_at_ + interval : 0, retry_at_);
        }
        if (due > nowEpoch())
            std::cerr << "[skinstrack] Next refresh at " << formatIsoUtc(due) << std::endl;
        if (!sleepUntil(due)) return;

        std::string error;
        switch (fetchOnce(error)) {
            case FetchResult::Ok:
                backoff = std::min(SKINSTRACK_MIN_BACKOFF_SECONDS, interval);
                break;

            case FetchResult::Unauthorized: {
                // Not persisted: after a restart with a fixed key, fetch right away.
                std::cerr << "[skinstrack] " << error << std::endl;
                std::lock_guard<std::mutex> lock(mutex_);
                last_error_ = error;
                return;
            }

            case FetchResult::HoldOff:   // 429 / 400: retrying sooner cannot help
                holdOff(nowEpoch() + interval, error);
                break;

            case FetchResult::Failed:    // network / 5xx / bad body: back off
                holdOff(nowEpoch() + backoff, error);
                backoff = std::min(backoff * 2, interval);
                break;
        }
    }
}
