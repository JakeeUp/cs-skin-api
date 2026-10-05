#pragma once
#include <optional>
#include <string>
#include <utility>

struct Config {
    std::string skinstrack_api_key;
    int         skinstrack_refresh_hours = 24;
    std::string skinstrack_cache_file    = "data/skinstrack-items.json";
    int         port                     = 8080;
    std::string allowed_origin;          // empty = any origin
    std::string ca_bundle;               // empty = libcurl default
};

// Parses one KEY=VALUE line from a .env file. Returns nothing for blank
// lines, comments, and malformed lines. Surrounding quotes are stripped.
std::optional<std::pair<std::string, std::string>> parseEnvLine(const std::string& line);

// Reads `envPath` (if present), then lets real environment variables
// override it. Never logs secret values.
Config loadConfig(const std::string& envPath = ".env");
