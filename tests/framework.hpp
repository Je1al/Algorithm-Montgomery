// SPDX-License-Identifier: MIT
//
// A dependency-free micro test harness. Each test executable links no external
// libraries; it just includes this header, registers checks, and returns the
// summary as its exit code (0 = all passed). Kept tiny on purpose so the whole
// project builds with nothing but a C++17 compiler.

#pragma once

#include <cstdio>
#include <string>

namespace montx::test {

struct Suite {
    const char* name;
    int passed = 0;
    int failed = 0;

    explicit Suite(const char* n) : name(n) {
        std::printf("== %s ==\n", name);
    }

    void check(bool cond, const std::string& what) {
        if (cond) {
            ++passed;
        } else {
            ++failed;
            std::printf("  [FAIL] %s\n", what.c_str());
        }
    }

    template <typename A, typename B>
    void eq(const A& got, const B& want, const std::string& what) {
        check(got == want, what);
        if (!(got == want)) {
            std::printf("         got=%s want=%s\n", to_s(got).c_str(), to_s(want).c_str());
        }
    }

    int summary() const {
        std::printf("-- %s: %d passed, %d failed\n\n", name, passed, failed);
        return failed == 0 ? 0 : 1;
    }

private:
    static std::string to_s(const std::string& s) { return s; }
    static std::string to_s(const char* s) { return s; }
    template <typename T>
    static std::string to_s(const T& v) { return std::to_string(v); }
};

}  // namespace montx::test

#define MONTX_CHECK(suite, cond) (suite).check((cond), #cond)
