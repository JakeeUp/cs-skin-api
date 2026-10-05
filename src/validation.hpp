#pragma once
#include <optional>
#include <string>

// Largest dollar amount any endpoint accepts.
constexpr double MAX_BUDGET_USD = 10000.0;

// Longest search term forwarded to Steam.
constexpr size_t MAX_QUERY_LENGTH = 64;

// Parses a dollar amount from user input. Rejects non-numbers, trailing
// garbage, NaN/infinity, negatives, and anything above MAX_BUDGET_USD.
std::optional<double> parseDollars(const std::string& text);

// Range-checks a dollar amount that arrived as a JSON number.
std::optional<double> checkDollars(double value);

// Rounds dollars to whole cents.
int toCents(double dollars);

// True for a non-empty, printable search term within MAX_QUERY_LENGTH.
bool isValidQuery(const std::string& q);
