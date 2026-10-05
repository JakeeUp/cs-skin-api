#include "optimizer.hpp"
#include <algorithm>

// ─── 0/1 Knapsack Budget Optimizer ─────────────────────────
//
// Selects the combination of skins that maximizes total value spent
// without exceeding the budget — a classic 0/1 knapsack problem.
//
// Uses dynamic programming for budgets <= $500 with <= 150 items.
// Falls back to a greedy heuristic (largest-first) for larger inputs
// where DP would be impractical.

std::vector<Skin> knapsackOptimize(const std::vector<Skin>& allItems, int capacity) {
    if (capacity <= 0) return {};

    // A negative price would index outside the DP table; such listings are bogus anyway.
    std::vector<Skin> items;
    items.reserve(allItems.size());
    for (const auto& s : allItems)
        if (s.price_cents >= 0) items.push_back(s);

    int n = static_cast<int>(items.size());

    if (usesGreedy(capacity, n)) {
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
