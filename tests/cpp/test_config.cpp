#include "config.hpp"
#include "test_harness.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ─── Environment helpers (portable, restore afterwards) ────

static void setEnv(const std::string& key, const std::optional<std::string>& value) {
#ifdef _WIN32
    _putenv_s(key.c_str(), value ? value->c_str() : "");  // "" removes it
#else
    if (value) setenv(key.c_str(), value->c_str(), 1);
    else       unsetenv(key.c_str());
#endif
}

static const std::vector<std::string> CONFIG_KEYS = {
    "SKINSTRACK_API_KEY", "SKINSTRACK_REFRESH_HOURS", "SKINSTRACK_CACHE_FILE",
    "PORT", "ALLOWED_ORIGIN", "CURL_CA_BUNDLE",
};

// Clears every config variable for the lifetime of the guard, then puts
// the original values back.
struct EnvGuard {
    std::vector<std::pair<std::string, std::optional<std::string>>> saved;
    EnvGuard() {
        for (const auto& k : CONFIG_KEYS) {
            const char* v = std::getenv(k.c_str());
            saved.emplace_back(k, v ? std::optional<std::string>(v) : std::nullopt);
            setEnv(k, std::nullopt);
        }
    }
    ~EnvGuard() {
        for (const auto& [k, v] : saved) setEnv(k, v);
    }
};

// Writes `contents` to a unique temp file and deletes it on destruction.
struct TempEnvFile {
    fs::path path;
    explicit TempEnvFile(const std::string& contents) {
        static int counter = 0;
        path = fs::temp_directory_path() /
               ("skin_config_test_" + std::to_string(counter++) + ".env");
        std::ofstream(path) << contents;
    }
    ~TempEnvFile() { std::error_code ec; fs::remove(path, ec); }
};

using KV = std::pair<std::string, std::string>;

static std::optional<KV> kv(const std::string& k, const std::string& v) { return KV{k, v}; }

// ─── parseEnvLine ──────────────────────────────────────────

TEST(parse_skips_blank_and_comments) {
    CHECK(!parseEnvLine(""));
    CHECK(!parseEnvLine("   \t  "));
    CHECK(!parseEnvLine("\r"));
    CHECK(!parseEnvLine("# PORT=1"));
    CHECK(!parseEnvLine("   # indented comment"));
}

TEST(parse_simple_pairs) {
    CHECK(parseEnvLine("PORT=9090") == kv("PORT", "9090"));
    CHECK(parseEnvLine("  PORT = 9090  ") == kv("PORT", "9090"));
    CHECK(parseEnvLine("PORT=9090\r") == kv("PORT", "9090"));  // CRLF files
    CHECK(parseEnvLine("EMPTY=") == kv("EMPTY", ""));
}

TEST(parse_export_prefix) {
    CHECK(parseEnvLine("export PORT=9090") == kv("PORT", "9090"));
    CHECK(parseEnvLine("export   KEY = v") == kv("KEY", "v"));
}

TEST(parse_strips_matching_quotes) {
    CHECK(parseEnvLine("KEY=\"quoted value\"") == kv("KEY", "quoted value"));
    CHECK(parseEnvLine("KEY='single'") == kv("KEY", "single"));
    CHECK(parseEnvLine("KEY=\"\"") == kv("KEY", ""));
    CHECK(parseEnvLine("KEY=\"mismatched'") == kv("KEY", "\"mismatched'"));
    CHECK(parseEnvLine("KEY=\"") == kv("KEY", "\""));
}

TEST(parse_keeps_equals_in_value) {
    CHECK(parseEnvLine("URL=https://x.test/?a=1&b=2") == kv("URL", "https://x.test/?a=1&b=2"));
    CHECK(parseEnvLine("K==v") == kv("K", "=v"));
}

TEST(parse_rejects_missing_key_or_equals) {
    CHECK(!parseEnvLine("=value"));
    CHECK(!parseEnvLine("JUSTAKEY"));
    CHECK(!parseEnvLine("export"));
}

// ─── loadConfig ────────────────────────────────────────────

TEST(load_defaults_when_file_missing) {
    EnvGuard env;
    Config c = loadConfig((fs::temp_directory_path() / "skin_config_does_not_exist.env").string());
    CHECK_EQ(c.port, 8080);
    CHECK_EQ(c.skinstrack_refresh_hours, 24);
    CHECK_EQ(c.skinstrack_cache_file, std::string("data/skinstrack-items.json"));
    CHECK(c.skinstrack_api_key.empty());
    CHECK(c.allowed_origin.empty());
    CHECK(c.ca_bundle.empty());
}

TEST(load_reads_env_file) {
    EnvGuard env;
    TempEnvFile file(
        "# local settings\n"
        "\n"
        "SKINSTRACK_API_KEY=filekey\n"
        "export PORT=9090\n"
        "SKINSTRACK_REFRESH_HOURS=12\n"
        "SKINSTRACK_CACHE_FILE='cache/items.json'\n"
        "ALLOWED_ORIGIN=\"https://example.com\"\n"
        "CURL_CA_BUNDLE=C:/certs/ca.pem\n"
        "UNRELATED=ignored\n");
    Config c = loadConfig(file.path.string());
    CHECK_EQ(c.skinstrack_api_key, std::string("filekey"));
    CHECK_EQ(c.port, 9090);
    CHECK_EQ(c.skinstrack_refresh_hours, 12);
    CHECK_EQ(c.skinstrack_cache_file, std::string("cache/items.json"));
    CHECK_EQ(c.allowed_origin, std::string("https://example.com"));
    CHECK_EQ(c.ca_bundle, std::string("C:/certs/ca.pem"));
}

TEST(load_rejects_out_of_range_numbers) {
    EnvGuard env;
    TempEnvFile file("PORT=70000\nSKINSTRACK_REFRESH_HOURS=0\n");
    Config c = loadConfig(file.path.string());
    CHECK_EQ(c.port, 8080);
    CHECK_EQ(c.skinstrack_refresh_hours, 24);

    TempEnvFile bad("PORT=abc\n");
    CHECK_EQ(loadConfig(bad.path.string()).port, 8080);

    TempEnvFile junk("PORT=80abc\n");
    CHECK_EQ(loadConfig(junk.path.string()).port, 8080);
}

TEST(env_vars_override_file) {
    EnvGuard env;
    TempEnvFile file("SKINSTRACK_API_KEY=filekey\nPORT=9090\nALLOWED_ORIGIN=https://file.test\n");
    setEnv("SKINSTRACK_API_KEY", std::string("envkey"));
    setEnv("PORT", std::string("7070"));
    Config c = loadConfig(file.path.string());
    CHECK_EQ(c.skinstrack_api_key, std::string("envkey"));
    CHECK_EQ(c.port, 7070);
    CHECK_EQ(c.allowed_origin, std::string("https://file.test"));  // not overridden
}

TEST(env_guard_restores_variables) {
    setEnv("PORT", std::string("1234"));
    {
        EnvGuard env;
        CHECK(std::getenv("PORT") == nullptr);
    }
    const char* restored = std::getenv("PORT");
    CHECK(restored != nullptr && std::string(restored) == "1234");
    setEnv("PORT", std::nullopt);
}

int main() { return runTests(); }
