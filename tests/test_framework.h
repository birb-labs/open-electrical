// =============================================================================
//  test_framework.h - Tiny header-only test harness (Catch2-style macros).
//
//  Chosen over GoogleTest/Catch2 because the build must work fully offline and
//  neither framework is vendored. Self-registering TEST_CASEs, a CHECK family,
//  and a runner (tf::runAll) that returns non-zero on any failure.
// =============================================================================
#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace tf {

struct TestCase { std::string name; std::function<void()> fn; };

inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
inline int& failures() { static int f = 0; return f; }
inline int& checks()   { static int c = 0; return c; }

struct Registrar {
    Registrar(const char* n, std::function<void()> f) { registry().push_back({ n, f }); }
};

inline void reportFail(const char* file, int line, const std::string& msg) {
    ++failures();
    std::cerr << "    FAIL " << file << ":" << line << "  " << msg << "\n";
}

inline int runAll() {
    int passed = 0;
    for (auto& t : registry()) {
        const int before = failures();
        try {
            t.fn();
        } catch (const std::exception& e) {
            reportFail("?", 0, std::string("exception: ") + e.what());
        } catch (...) {
            reportFail("?", 0, "unknown exception");
        }
        if (failures() == before) { ++passed; }
        else { std::cerr << "  [FAILED] " << t.name << "\n"; }
    }
    std::cout << passed << "/" << registry().size() << " tests passed, "
              << checks() << " checks, " << failures() << " failure(s)\n";
    return failures() ? 1 : 0;
}

} // namespace tf

#define TEST_CASE(name)                                                        \
    static void name();                                                        \
    static ::tf::Registrar reg_##name(#name, name);                            \
    static void name()

#define CHECK(cond)                                                            \
    do { ++::tf::checks();                                                     \
         if (!(cond)) ::tf::reportFail(__FILE__, __LINE__, "CHECK: " #cond);   \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do { ++::tf::checks();                                                     \
         if (!((a) == (b)))                                                    \
             ::tf::reportFail(__FILE__, __LINE__, "CHECK_EQ: " #a " == " #b);  \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                  \
    do { ++::tf::checks();                                                     \
         if (std::fabs((a) - (b)) > (eps))                                     \
             ::tf::reportFail(__FILE__, __LINE__, "CHECK_NEAR: " #a " ~ " #b); \
    } while (0)
