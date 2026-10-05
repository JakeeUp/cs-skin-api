#pragma once
#include "skin.hpp"
#include <vector>

// Above these limits the DP table gets too large and the greedy fallback runs.
constexpr int KNAPSACK_MAX_CAPACITY = 50000;  // cents
constexpr int KNAPSACK_MAX_ITEMS    = 150;

inline bool usesGreedy(int capacity, int itemCount) {
    return capacity > KNAPSACK_MAX_CAPACITY || itemCount > KNAPSACK_MAX_ITEMS;
}

// Selects the subset of items whose total price is as close to `capacity`
// as possible without exceeding it (0/1 knapsack, value == cost).
std::vector<Skin> knapsackOptimize(const std::vector<Skin>& items, int capacity);
