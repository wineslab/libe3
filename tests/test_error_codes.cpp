/**
 * @file test_error_codes.cpp
 * @brief Unit tests for shared error code mappings
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/error_codes.h"
#include "libe3/types.hpp"

using namespace libe3;

TEST(C_API_error_to_string) {
    /* C API returns uppercase symbol names as declared by the X-macro */
    ASSERT_STREQ(e3_error_to_string(E3_SUCCESS), "SUCCESS");
    ASSERT_STREQ(e3_error_to_string(E3_TIMEOUT), "TIMEOUT");
    ASSERT_STREQ(e3_error_to_string(E3_NOT_FOUND), "NOT_FOUND");
}

TEST(CXX_ErrorCodeToString_wrapper) {
    /* C++ wrapper returns the symbol name as well */
    ASSERT_STREQ(ErrorCodeToString(ErrorCode::SUCCESS), "SUCCESS");
    ASSERT_STREQ(ErrorCodeToString(ErrorCode::TIMEOUT), "TIMEOUT");
    ASSERT_STREQ(ErrorCodeToString(ErrorCode::NOT_FOUND), "NOT_FOUND");
}

int main() { return RUN_ALL_TESTS(); }
