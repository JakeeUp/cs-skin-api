// Minimal assert-style test harness: no dependencies, no downloads.
//
//   TEST(something_works) { CHECK(1 + 1 == 2); CHECK_EQ(f(), 42); }
//   int main() { return runTests(); }
//
// Failed checks are reported and counted; the test keeps running so one
// run shows every failure. The process exits non-zero if anything failed.
#pragma once
#include <cstdio>
#include <iostream>
#include <vector>

namespace th {

struct TestCase { const char* name; void (*fn)(); };

inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
inline int& failures() { static int f = 0; return f; }

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

}  // namespace th

#define TEST(name)                                             \
    static void name();                                        \
    static th::Registrar th_reg_##name(#name, name);           \
    static void name()

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << __FILE__ << ":" << __LINE__                           \
                      << ": CHECK failed: " #cond << std::endl;                \
            ++th::failures();                                                  \
        }                                                                      \
    } while (0)

#define CHECK_EQ(actual, expected)                                             \
    do {                                                                       \
        const auto& th_a = (actual);                                           \
        const auto& th_e = (expected);                                         \
        if (!(th_a == th_e)) {                                                 \
            std::cerr << __FILE__ << ":" << __LINE__                           \
                      << ": CHECK_EQ failed: " #actual " == " #expected        \
                      << "\n    actual:   " << th_a                            \
                      << "\n    expected: " << th_e << std::endl;            \
            ++th::failures();                                                  \
        }                                                                      \
    } while (0)

inline int runTests() {
    int failedTests = 0;
    for (const auto& t : th::registry()) {
        int before = th::failures();
        t.fn();
        bool ok = th::failures() == before;
        if (!ok) ++failedTests;
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", t.name);
    }
    std::printf("%d/%d tests passed\n",
                static_cast<int>(th::registry().size()) - failedTests,
                static_cast<int>(th::registry().size()));
    return failedTests == 0 ? 0 : 1;
}
