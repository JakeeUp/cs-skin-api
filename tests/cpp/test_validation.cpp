#include "validation.hpp"
#include "test_harness.hpp"
#include <cmath>
#include <limits>
#include <string>

TEST(parse_dollars_accepts_numbers) {
    CHECK_EQ(parseDollars("12.5").value_or(-1), 12.5);
    CHECK_EQ(parseDollars("0").value_or(-1), 0.0);
    CHECK_EQ(parseDollars("1e3").value_or(-1), 1000.0);
    CHECK_EQ(parseDollars("10000").value_or(-1), 10000.0);
}

TEST(parse_dollars_rejects_garbage) {
    CHECK(!parseDollars(""));
    CHECK(!parseDollars("abc"));
    CHECK(!parseDollars("5x"));
    CHECK(!parseDollars("12.5 "));
    CHECK(!parseDollars("00000000000000001"));  // longer than 16 chars
}

TEST(parse_dollars_rejects_non_finite) {
    CHECK(!parseDollars("nan"));
    CHECK(!parseDollars("NaN"));
    CHECK(!parseDollars("inf"));
    CHECK(!parseDollars("-inf"));
    CHECK(!parseDollars("infinity"));
}

TEST(parse_dollars_rejects_out_of_range) {
    CHECK(!parseDollars("-1"));
    CHECK(!parseDollars("-0.01"));
    CHECK(!parseDollars("10000.01"));
    CHECK(!parseDollars("1e5"));
}

TEST(check_dollars_range) {
    CHECK_EQ(checkDollars(0.0).value_or(-1), 0.0);
    CHECK_EQ(checkDollars(MAX_BUDGET_USD).value_or(-1), MAX_BUDGET_USD);
    CHECK_EQ(checkDollars(42.42).value_or(-1), 42.42);
    CHECK(!checkDollars(-0.01));
    CHECK(!checkDollars(MAX_BUDGET_USD + 0.01));
    CHECK(!checkDollars(std::numeric_limits<double>::quiet_NaN()));
    CHECK(!checkDollars(std::numeric_limits<double>::infinity()));
    CHECK(!checkDollars(-std::numeric_limits<double>::infinity()));
}

TEST(to_cents_rounds) {
    CHECK_EQ(toCents(19.99), 1999);  // 19.99 * 100 = 1998.9999...
    CHECK_EQ(toCents(0.29), 29);     // 0.29 * 100 = 28.999...
    CHECK_EQ(toCents(0.0), 0);
    CHECK_EQ(toCents(12.5), 1250);
    CHECK_EQ(toCents(MAX_BUDGET_USD), 1000000);
}

TEST(query_length_limits) {
    CHECK(!isValidQuery(""));
    CHECK(isValidQuery("a"));
    CHECK(isValidQuery(std::string(64, 'a')));
    CHECK(!isValidQuery(std::string(65, 'a')));
}

TEST(query_rejects_control_chars) {
    CHECK(!isValidQuery("ak\n47"));
    CHECK(!isValidQuery("ak\t47"));
    CHECK(!isValidQuery("ak\r"));
    CHECK(!isValidQuery(std::string("ak\0" "47", 5)));
    CHECK(!isValidQuery("\x7f"));
}

TEST(query_accepts_normal_names) {
    CHECK(isValidQuery("AK-47 | Redline (Field-Tested)"));
    CHECK(isValidQuery("\xE2\x98\x85 Karambit"));  // UTF-8 star
}

int main() { return runTests(); }
