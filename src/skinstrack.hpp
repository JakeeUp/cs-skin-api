#pragma once
#include "config.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

// One item's price from SkinsTrack (https://skinstrack.com/api-docs).
struct SkinstrackPrice {
    int         price_cents = 0;
    int         liquidity   = 0;
    int         count       = 0;   // listings on the provider
    int         volume      = 0;   // recent sales on the provider
    std::string updated_at;        // provider timestamp, ISO-8601 as sent
    std::string icon_url;
};

// market_hash_name -> price
using SkinstrackPriceMap = std::unordered_map<std::string, SkinstrackPrice>;

// What the on-disk cache holds: the prices plus when they were fetched and,
// after a failed fetch, the earliest time another call may be made.
struct SkinstrackSnapshot {
    SkinstrackPriceMap prices;
    long long          fetched_at = 0;   // epoch seconds, 0 = never
    long long          retry_at   = 0;   // epoch seconds, 0 = no hold-off
};

// ─── Pure helpers (no I/O) ─────────────────────────────────

// Parses a SkinsTrack /v2/free/items body. Accepts either
// {"items":[...]} or a bare [...] array. For each item the "steam" provider
// entry is preferred, else the first entry with a usable price. Items
// without a name or a usable price are skipped; malformed input yields an
// empty map rather than throwing.
SkinstrackPriceMap parseSkinstrackItems(const std::string& body);

// Parses an API body or a cache snapshot: the items as above, plus the
// top-level "fetched_at" / "retry_at" epoch seconds when present (else 0).
SkinstrackSnapshot parseSkinstrackSnapshot(const std::string& body);

// Serializes a snapshot in the same shape the API returns (one price entry
// per item) plus "fetched_at"/"retry_at", so it parses back losslessly.
std::string serializeSkinstrackSnapshot(const SkinstrackSnapshot& snapshot);

// Formats epoch seconds as ISO-8601 UTC, e.g. "2025-11-25T10:30:00Z".
std::string formatIsoUtc(long long epoch_seconds);

// ─── Price store ───────────────────────────────────────────

struct SkinstrackStatus {
    bool        configured    = false;  // an API key is set
    int         items         = 0;
    long long   fetched_at    = 0;      // epoch seconds, 0 = no data
    int         refresh_hours = 0;
    std::string last_error;             // empty = none
};

// Thread-safe SkinsTrack price cache.
//
// start() loads the on-disk snapshot synchronously, then (only if an API key
// is configured) runs a background thread that fetches the full item list
// whenever the snapshot is older than refresh_hours. The fetch time is
// persisted, so restarts do not spend extra calls from the 50/month quota.
// Without a key the store serves whatever snapshot is on disk, or nothing.
class SkinstrackStore {
public:
    explicit SkinstrackStore(const Config& config);
    ~SkinstrackStore();  // signals the refresh thread and joins it

    SkinstrackStore(const SkinstrackStore&)            = delete;
    SkinstrackStore& operator=(const SkinstrackStore&) = delete;

    void start();

    std::optional<SkinstrackPrice> find(const std::string& market_hash_name) const;
    SkinstrackStatus               status() const;

private:
    enum class FetchResult { Ok, Unauthorized, HoldOff, Failed };

    void        run();
    void        loadCache();
    bool        saveCache(const SkinstrackSnapshot& snapshot) const;
    FetchResult fetchOnce(std::string& error);
    void        holdOff(long long until, const std::string& error);
    bool        sleepUntil(long long epoch_seconds);  // false when stopping

    const std::string api_key_;
    const int         refresh_hours_;
    const std::string cache_file_;

    mutable std::mutex                        mutex_;
    std::condition_variable                   wake_;
    std::shared_ptr<const SkinstrackPriceMap> prices_;
    long long                                 fetched_at_ = 0;
    long long                                 retry_at_   = 0;
    std::string                               last_error_;
    bool                                      stopping_   = false;
    std::thread                               thread_;
};
