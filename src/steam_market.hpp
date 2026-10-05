#pragma once
#include "skin.hpp"
#include <set>
#include <string>
#include <vector>

// Fetches `pages` pages of Steam market results for a query across two sort
// orders (popular + price), keeping skins priced within [min_cents, max_cents].
void fetchQuery(
    const std::string&     query,
    int                    pages,
    int                    min_cents,
    int                    max_cents,
    std::vector<Skin>&     skins,
    std::set<std::string>& seen
);

// Fetches options for one loadout slot, interleaving the best result from
// each weapon so the slot shows variety rather than one weapon's listings.
std::vector<Skin> fetchSlotOptions(
    const std::vector<std::string>& queries,
    int                             budget_cents,
    int                             max_options = 5
);
