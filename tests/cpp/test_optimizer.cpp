#include "optimizer.hpp"
#include "test_harness.hpp"
#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

static std::vector<Skin> makeSkins(const std::vector<int>& prices) {
    std::vector<Skin> skins;
    for (size_t i = 0; i < prices.size(); i++) {
        Skin s{};
        s.name = "skin" + std::to_string(i);
        s.price_cents = prices[i];
        skins.push_back(s);
    }
    return skins;
}

static int total(const std::vector<Skin>& picked) {
    int sum = 0;
    for (const auto& s : picked) sum += s.price_cents;
    return sum;
}

// Every pick must be a distinct input item (names are unique in makeSkins).
static bool distinctInputs(const std::vector<Skin>& picked, const std::vector<Skin>& items) {
    std::set<std::string> seen;
    for (const auto& s : picked) {
        if (!seen.insert(s.name).second) return false;
        auto it = std::find_if(items.begin(), items.end(),
                               [&](const Skin& x) { return x.name == s.name; });
        if (it == items.end() || it->price_cents != s.price_cents) return false;
    }
    return true;
}

// Best achievable total by trying every subset.
static int bruteForceBest(const std::vector<int>& prices, int capacity) {
    int best = 0;
    for (uint32_t mask = 0; mask < (1u << prices.size()); mask++) {
        int sum = 0;
        for (size_t i = 0; i < prices.size(); i++)
            if (mask & (1u << i)) sum += prices[i];
        if (sum <= capacity) best = std::max(best, sum);
    }
    return best;
}

TEST(uses_greedy_thresholds) {
    CHECK(!usesGreedy(KNAPSACK_MAX_CAPACITY, KNAPSACK_MAX_ITEMS));
    CHECK(usesGreedy(KNAPSACK_MAX_CAPACITY + 1, 1));
    CHECK(usesGreedy(100, KNAPSACK_MAX_ITEMS + 1));
    CHECK(!usesGreedy(0, 0));
}

TEST(dp_spends_exact_budget) {
    auto items = makeSkins({500, 300, 250, 200});
    auto picked = knapsackOptimize(items, 750);
    CHECK_EQ(total(picked), 750);
    CHECK(distinctInputs(picked, items));
}

TEST(dp_beats_largest_first) {
    // Largest-first would take 600 and stop; the optimum is 400 + 350.
    auto items = makeSkins({600, 400, 350});
    auto picked = knapsackOptimize(items, 750);
    CHECK_EQ(total(picked), 750);
    CHECK_EQ(picked.size(), size_t(2));
}

TEST(dp_item_equal_to_capacity) {
    auto picked = knapsackOptimize(makeSkins({750}), 750);
    CHECK_EQ(picked.size(), size_t(1));
    CHECK_EQ(total(picked), 750);
}

TEST(dp_at_capacity_limit) {
    auto items = makeSkins({30000, 25000, 20000});
    auto picked = knapsackOptimize(items, KNAPSACK_MAX_CAPACITY);
    CHECK_EQ(total(picked), 50000);
    CHECK(distinctInputs(picked, items));
}

TEST(empty_input) {
    CHECK(knapsackOptimize({}, 1000).empty());
    CHECK(knapsackOptimize({}, 0).empty());
}

TEST(zero_capacity) {
    CHECK(knapsackOptimize(makeSkins({1, 2, 3}), 0).empty());
}

TEST(capacity_below_every_item) {
    CHECK(knapsackOptimize(makeSkins({500, 300, 250}), 249).empty());
}

TEST(no_duplicate_picks) {
    // A single cheap item must not be reused to fill the budget.
    auto one = makeSkins({100});
    auto picked = knapsackOptimize(one, 1000);
    CHECK_EQ(picked.size(), size_t(1));
    CHECK(distinctInputs(picked, one));

    // Equal prices: two distinct items, not the same one twice.
    auto same = makeSkins({100, 100, 100});
    picked = knapsackOptimize(same, 250);
    CHECK_EQ(picked.size(), size_t(2));
    CHECK(distinctInputs(picked, same));
}

TEST(dp_matches_brute_force) {
    // Deterministic LCG so failures are reproducible.
    uint32_t seed = 12345;
    auto next = [&](int mod) { seed = seed * 1103515245u + 12345u; return int((seed >> 16) % mod); };

    for (int round = 0; round < 200; round++) {
        int n = next(11);  // 0..10 items
        std::vector<int> prices;
        for (int i = 0; i < n; i++) prices.push_back(1 + next(2000));
        int capacity = next(5000);

        auto items = makeSkins(prices);
        auto picked = knapsackOptimize(items, capacity);
        int expected = bruteForceBest(prices, capacity);
        CHECK_EQ(total(picked), expected);
        CHECK(total(picked) <= capacity);
        CHECK(distinctInputs(picked, items));
    }
}

TEST(greedy_above_capacity_limit) {
    // 60000 > 50000 forces greedy: takes 40000, skips 30000 and 25000, takes 5000.
    auto items = makeSkins({30000, 5000, 40000, 25000});
    int capacity = 60000;
    CHECK(usesGreedy(capacity, int(items.size())));
    auto picked = knapsackOptimize(items, capacity);
    CHECK_EQ(total(picked), 45000);
    CHECK_EQ(picked.size(), size_t(2));
    CHECK(total(picked) <= capacity);
    CHECK(distinctInputs(picked, items));
}

TEST(greedy_above_item_limit) {
    std::vector<int> prices(KNAPSACK_MAX_ITEMS + 1, 100);
    auto items = makeSkins(prices);
    CHECK(usesGreedy(1050, int(items.size())));
    auto picked = knapsackOptimize(items, 1050);
    CHECK_EQ(picked.size(), size_t(10));
    CHECK_EQ(total(picked), 1000);
    CHECK(distinctInputs(picked, items));
}

TEST(greedy_never_exceeds_capacity) {
    auto items = makeSkins({49999, 49999, 30001, 20000, 1});
    auto picked = knapsackOptimize(items, 100000);
    CHECK(total(picked) <= 100000);
    CHECK(distinctInputs(picked, items));
}

TEST(negative_capacity_returns_nothing) {
    auto items = makeSkins({100, 200});
    CHECK(knapsackOptimize(items, -1).empty());
    CHECK(knapsackOptimize(items, -500).empty());
}

TEST(negative_prices_are_ignored) {
    auto items = makeSkins({-50, 300, 200});
    auto picked = knapsackOptimize(items, 500);
    CHECK_EQ(total(picked), 500);
    for (const auto& s : picked) CHECK(s.price_cents >= 0);
}

int main() { return runTests(); }
