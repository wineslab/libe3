/**
 * @file test_framework.hpp
 * @brief Lightweight test framework for libe3 unit tests
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_TEST_FRAMEWORK_HPP
#define LIBE3_TEST_FRAMEWORK_HPP

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <cstdlib>

namespace libe3_test {

// Test result tracking
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

inline std::vector<TestResult>& get_results() {
    static std::vector<TestResult> results;
    return results;
}

inline int& test_count() {
    static int count = 0;
    return count;
}

inline int& pass_count() {
    static int count = 0;
    return count;
}

inline int& fail_count() {
    static int count = 0;
    return count;
}

// Test registration
struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

struct TestRegistrar {
    TestRegistrar(const std::string& name, std::function<void()> func) {
        get_tests().push_back({name, func});
    }
};

// Assertion functions
inline void report_pass(const std::string& test_name) {
    ++test_count();
    ++pass_count();
    get_results().push_back({test_name, true, ""});
}

inline void report_fail(const std::string& test_name, const std::string& msg) {
    ++test_count();
    ++fail_count();
    get_results().push_back({test_name, false, msg});
}

// Run all tests and report
inline int run_all_tests() {
    std::cout << "\n========================================\n";
    std::cout << "Running " << get_tests().size() << " test(s)\n";
    std::cout << "========================================\n\n";
    
    for (auto& test : get_tests()) {
        std::cout << "[RUN ] " << test.name << "\n";
        try {
            test.func();
            std::cout << "[PASS] " << test.name << "\n";
            report_pass(test.name);
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << test.name << "\n";
            std::cout << "       Exception: " << e.what() << "\n";
            report_fail(test.name, e.what());
        } catch (...) {
            std::cout << "[FAIL] " << test.name << "\n";
            std::cout << "       Unknown exception\n";
            report_fail(test.name, "Unknown exception");
        }
    }
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << pass_count() << " passed, " 
              << fail_count() << " failed out of " << test_count() << "\n";
    std::cout << "========================================\n\n";
    
    return fail_count() > 0 ? 1 : 0;
}

} // namespace libe3_test

// ============================================================================
// Test Macros
// ============================================================================

#define TEST(name) \
    void test_##name(); \
    static libe3_test::TestRegistrar registrar_##name(#name, test_##name); \
    void test_##name()

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::ostringstream oss; \
            oss << __FILE__ << ":" << __LINE__ << ": Assertion failed: " #cond; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (_a != _b) { \
            std::ostringstream oss; \
            oss << __FILE__ << ":" << __LINE__ << ": Assertion failed: " \
                << #a << " == " << #b << " (got " << _a << " vs " << _b << ")"; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (_a == _b) { \
            std::ostringstream oss; \
            oss << __FILE__ << ":" << __LINE__ << ": Assertion failed: " \
                << #a << " != " << #b; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_GT(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (!(_a > _b)) { \
            std::ostringstream oss; \
            oss << __FILE__ << ":" << __LINE__ << ": Assertion failed: " \
                << #a << " > " << #b << " (got " << _a << " vs " << _b << ")"; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_GE(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (!(_a >= _b)) { \
            std::ostringstream oss; \
            oss << __FILE__ << ":" << __LINE__ << ": Assertion failed: " \
                << #a << " >= " << #b << " (got " << _a << " vs " << _b << ")"; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_LT(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (!(_a < _b)) { \
            std::ostringstream oss; \
            oss << __FILE__ << ":" << __LINE__ << ": Assertion failed: " \
                << #a << " < " << #b << " (got " << _a << " vs " << _b << ")"; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_LE(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (!(_a <= _b)) { \
            std::ostringstream oss; \
            oss << __FILE__ << ":" << __LINE__ << ": Assertion failed: " \
                << #a << " <= " << #b << " (got " << _a << " vs " << _b << ")"; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_STREQ(a, b) \
    do { \
        std::string _a(a); \
        std::string _b(b); \
        if (_a != _b) { \
            std::ostringstream oss; \
            oss << __FILE__ << ":" << __LINE__ << ": Assertion failed: " \
                << #a << " == " << #b << "\n  Got: \"" << _a << "\"\n  Expected: \"" << _b << "\""; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_THROWS(expr, exctype) \
    do { \
        bool caught = false; \
        try { expr; } \
        catch (const exctype&) { caught = true; } \
        catch (...) {} \
        if (!caught) { \
            std::ostringstream oss; \
            oss << __FILE__ << ":" << __LINE__ << ": Expected " #exctype " from: " #expr; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_NO_THROW(expr) \
    do { \
        try { expr; } \
        catch (const std::exception& e) { \
            std::ostringstream oss; \
            oss << __FILE__ << ":" << __LINE__ << ": Unexpected exception: " << e.what(); \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define RUN_ALL_TESTS() libe3_test::run_all_tests()

#endif // LIBE3_TEST_FRAMEWORK_HPP
