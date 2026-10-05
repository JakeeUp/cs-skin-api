#pragma once
#include <string>
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
