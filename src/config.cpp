#include "config.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>

static std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    auto start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

std::optional<std::pair<std::string, std::string>> parseEnvLine(const std::string& line) {
    std::string t = trim(line);
    if (t.empty() || t[0] == '#') return std::nullopt;
    if (t.rfind("export ", 0) == 0) t = trim(t.substr(7));

    auto eq = t.find('=');
    if (eq == std::string::npos || eq == 0) return std::nullopt;

    std::string key   = trim(t.substr(0, eq));
    std::string value = trim(t.substr(eq + 1));
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\'')))
        value = value.substr(1, value.size() - 2);

    return std::make_pair(key, value);
}

static int toInt(const std::string& s, int fallback, int lo, int hi) {
    try {
        size_t used = 0;
        int v = std::stoi(s, &used);
        if (used == s.size() && v >= lo && v <= hi) return v;
    } catch (...) {}
    std::cerr << "[config] Ignoring out-of-range value '" << s << "'" << std::endl;
    return fallback;
}

Config loadConfig(const std::string& envPath) {
    std::map<std::string, std::string> vars;

    std::ifstream file(envPath);
    std::string line;
    while (std::getline(file, line))
        if (auto kv = parseEnvLine(line)) vars[kv->first] = kv->second;

    auto get = [&](const char* key) -> std::string {
        if (const char* env = std::getenv(key); env && *env) return env;
        auto it = vars.find(key);
        return it == vars.end() ? "" : it->second;
    };

    Config c;
    c.skinstrack_api_key = get("SKINSTRACK_API_KEY");
    if (auto v = get("SKINSTRACK_REFRESH_HOURS"); !v.empty())
        c.skinstrack_refresh_hours = toInt(v, c.skinstrack_refresh_hours, 1, 24 * 30);
    if (auto v = get("SKINSTRACK_CACHE_FILE"); !v.empty())
        c.skinstrack_cache_file = v;
    if (auto v = get("PORT"); !v.empty())
        c.port = toInt(v, c.port, 1, 65535);
    c.allowed_origin = get("ALLOWED_ORIGIN");
    c.ca_bundle      = get("CURL_CA_BUNDLE");

    std::cerr << "[config] port=" << c.port
              << " origin=" << (c.allowed_origin.empty() ? "*" : c.allowed_origin)
              << " skinstrack_key=" << (c.skinstrack_api_key.empty() ? "missing" : "set")
              << std::endl;
    return c;
}
