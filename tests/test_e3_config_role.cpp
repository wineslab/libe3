/**
 * @file test_e3_config_role.cpp
 * @brief Unit tests for the E3Role enum and the role/dapp-identification fields on E3Config.
 *
 * Part of issue #15: extend libe3 to support both RAN and dApp roles.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/types.hpp"

using namespace libe3;

TEST(E3Role_enum_values) {
    ASSERT_EQ(static_cast<int>(E3Role::RAN), 0);
    ASSERT_EQ(static_cast<int>(E3Role::DAPP), 1);
}

TEST(role_to_string_returns_lowercase_names) {
    ASSERT_STREQ(role_to_string(E3Role::RAN), "ran");
    ASSERT_STREQ(role_to_string(E3Role::DAPP), "dapp");
}

TEST(E3Config_default_role_is_RAN) {
    // Backward-compat invariant: any existing user constructing E3Config{}
    // must see role==RAN so today's RAN behaviour is preserved unchanged.
    E3Config cfg;
    ASSERT_TRUE(cfg.role == E3Role::RAN);
}

TEST(E3Config_dapp_identification_fields_have_sensible_defaults) {
    // dApp identification fields are only used when role==DAPP, but they must
    // be default-constructible so they don't add a constructor burden on
    // existing RAN users.
    E3Config cfg;
    ASSERT_TRUE(!cfg.dapp_name.empty());     // non-empty default for setup request
    ASSERT_TRUE(!cfg.dapp_version.empty());
    // vendor may be empty by default — that's fine; it's optional in the protocol
}

TEST(E3Config_role_is_assignable) {
    E3Config cfg;
    cfg.role = E3Role::DAPP;
    ASSERT_TRUE(cfg.role == E3Role::DAPP);

    cfg.role = E3Role::RAN;
    ASSERT_TRUE(cfg.role == E3Role::RAN);
}

TEST(E3Config_role_survives_copy) {
    E3Config src;
    src.role = E3Role::DAPP;
    src.dapp_name = "TestDApp";
    src.dapp_version = "9.9.9";
    src.vendor = "WinesLab";

    E3Config copy = src;
    ASSERT_TRUE(copy.role == E3Role::DAPP);
    ASSERT_STREQ(copy.dapp_name.c_str(), "TestDApp");
    ASSERT_STREQ(copy.dapp_version.c_str(), "9.9.9");
    ASSERT_STREQ(copy.vendor.c_str(), "WinesLab");
}

TEST(E3Config_role_survives_move) {
    E3Config src;
    src.role = E3Role::DAPP;
    src.dapp_name = "MovedDApp";

    E3Config moved = std::move(src);
    ASSERT_TRUE(moved.role == E3Role::DAPP);
    ASSERT_STREQ(moved.dapp_name.c_str(), "MovedDApp");
}

int main() {
    return RUN_ALL_TESTS();
}
