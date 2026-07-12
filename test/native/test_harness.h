#pragma once

#include <cstdio>
#include <cstring>

namespace pd_test {

inline int checks = 0;
inline int failures = 0;

inline void check(bool condition, const char* expression, const char* file, int line) {
    ++checks;
    if (condition) return;
    ++failures;
    std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expression);
}

inline int finish() {
    if (failures == 0) {
        std::printf("PASS: %d checks\n", checks);
        return 0;
    }
    std::fprintf(stderr, "FAIL: %d of %d checks\n", failures, checks);
    return 1;
}

}  // namespace pd_test

#define TEST_CASE(name) static void name()
#define CHECK(expression) pd_test::check((expression), #expression, __FILE__, __LINE__)
#define CHECK_EQ(actual, expected) \
    pd_test::check(((actual) == (expected)), #actual " == " #expected, __FILE__, __LINE__)
#define CHECK_STR_EQ(actual, expected) \
    pd_test::check((std::strcmp((actual), (expected)) == 0), #actual " == " #expected, __FILE__, __LINE__)

