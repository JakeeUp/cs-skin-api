#include "validation.hpp"
#include <cmath>
#include <cstdlib>

std::optional<double> checkDollars(double value) {
    if (!std::isfinite(value) || value < 0.0 || value > MAX_BUDGET_USD)
        return std::nullopt;
    return value;
}

std::optional<double> parseDollars(const std::string& text) {
    if (text.empty() || text.size() > 16) return std::nullopt;
    char* end = nullptr;
    double v = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size()) return std::nullopt;
    return checkDollars(v);
}

int toCents(double dollars) {
    return static_cast<int>(std::lround(dollars * 100.0));
}

bool isValidQuery(const std::string& q) {
    if (q.empty() || q.size() > MAX_QUERY_LENGTH) return false;
    for (unsigned char c : q)
        if (c < 0x20 || c == 0x7f) return false;  // control characters
    return true;
}
