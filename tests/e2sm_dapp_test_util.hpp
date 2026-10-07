/**
 * @file e2sm_dapp_test_util.hpp
 * @brief Shared helpers of the E2SM-DAPP codec tests: the golden vectors, the
 *        fixture builders, and a type-erased list of every golden case.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_E2SM_DAPP_TEST_UTIL_HPP
#define LIBE3_E2SM_DAPP_TEST_UTIL_HPP

#include "libe3/e2sm_dapp.hpp"

#define E2SM_DAPP_NS libe3::e2sm_dapp
#define E2SM_DAPP_SET_STRING(dst, text) dst.from_string(text)
#define E2SM_DAPP_SET_OCTETS(dst, ptr, n) dst.from_bytes(ptr, n)

#include "e2sm_dapp_fixtures.inc"
#include "e2sm_dapp_golden.hpp"

#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace e2sm_dapp_test {

using libe3::e2sm_dapp::asn_code;
using libe3::e2sm_dapp::ASN_ERROR_DECODE_FAIL;
using libe3::e2sm_dapp::ASN_ERROR_ENCODE_FAIL;
using libe3::e2sm_dapp::ASN_SUCCESS;

inline std::vector<uint8_t> from_hex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("odd hex length");
    }
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    auto nibble = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
        throw std::runtime_error("bad hex digit");
    };
    for (size_t i = 0; i < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>(nibble(hex[i]) * 16 + nibble(hex[i + 1])));
    }
    return out;
}

inline std::string to_hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (uint8_t b : v) {
        s.push_back(d[b >> 4]);
        s.push_back(d[b & 0x0f]);
    }
    return s;
}

inline uint64_t fnv1a64(const std::vector<uint8_t>& v) {
    uint64_t h = 1469598103934665603ULL;
    for (uint8_t b : v) {
        h ^= b;
        h *= 1099511628211ULL;
    }
    return h;
}

/// One golden vector: inline hex, or length plus FNV-1a 64 for the big ones.
struct Golden {
    bool                 inlined = false;
    std::vector<uint8_t> bytes;   ///< set when inlined
    size_t               length = 0;
    uint64_t             hash   = 0;
};

inline Golden golden(const std::string& name) {
    Golden g;
    for (const auto& v : libe3_e2sm_dapp_test::golden_vectors) {
        if (name == v.name) {
            g.inlined = true;
            g.bytes   = from_hex(v.hex);
            g.length  = g.bytes.size();
            g.hash    = fnv1a64(g.bytes);
            return g;
        }
    }
    for (const auto& h : libe3_e2sm_dapp_test::golden_hashes) {
        if (name == h.name) {
            g.length = h.length;
            g.hash   = h.fnv1a64;
            return g;
        }
    }
    throw std::runtime_error("no golden vector named " + name);
}

/// True when `bytes` is exactly the golden vector (hex compare, or length + hash).
inline bool matches_golden(const Golden& g, const std::vector<uint8_t>& bytes) {
    if (bytes.size() != g.length) return false;
    if (g.inlined) return bytes == g.bytes;
    return fnv1a64(bytes) == g.hash;
}

/// A golden case with its type erased: pack the fixture, or unpack some bytes into a fresh T.
struct Case {
    std::string name;
    /// Pack the fixture.
    std::function<asn_code(std::vector<uint8_t>&)> pack_fixture;
    /// Unpack into a fresh T. On success: *equal = (T == fixture), and *repacked = T.pack().
    std::function<asn_code(const uint8_t*, size_t, bool* equal, std::vector<uint8_t>* repacked)> unpack;
};

template <class T>
Case make_case(const std::string& name, T (*fixture)()) {
    Case c;
    c.name         = name;
    c.pack_fixture = [fixture](std::vector<uint8_t>& out) { return fixture().pack(out); };
    c.unpack       = [fixture](const uint8_t* p, size_t n, bool* equal, std::vector<uint8_t>* repacked) {
        T       v;
        asn_code rc = v.unpack(p, n);
        if (rc == ASN_SUCCESS) {
            if (equal != nullptr) *equal = (v == fixture());
            if (repacked != nullptr) {
                if (v.pack(*repacked) != ASN_SUCCESS) repacked->clear();
            }
        }
        return rc;
    };
    return c;
}

/// Every golden case, in the order of the task description.
inline const std::vector<Case>& all_cases() {
    namespace f = e2sm_dapp_fixtures;
    static const std::vector<Case> cases = {
        make_case("et_f1", &f::et_f1),
        make_case("ad_style1", &f::ad_style1),
        make_case("ad_style2", &f::ad_style2),
        make_case("ad_style_max", &f::ad_style_max),
        make_case("ih1_full", &f::ih1_full),
        make_case("ih1_min", &f::ih1_min),
        make_case("ih1_bounds", &f::ih1_bounds),
        make_case("ih2_full", &f::ih2_full),
        make_case("ih2_min", &f::ih2_min),
        make_case("im1_small", &f::im1_small),
        make_case("im1_one", &f::im1_one),
        make_case("im1_max", &f::im1_max),
        make_case("im2_empty", &f::im2_empty),
        make_case("im2_one", &f::im2_one),
        make_case("im2_multi", &f::im2_multi),
        make_case("im2_255", &f::im2_255),
        make_case("im2_256", &f::im2_256),
        make_case("ch1_full", &f::ch1_full),
        make_case("ch1_min", &f::ch1_min),
        make_case("ch1_bounds", &f::ch1_bounds),
        make_case("cm1_small", &f::cm1_small),
        make_case("cm1_one", &f::cm1_one),
        make_case("cm1_large", &f::cm1_large),
        make_case("co1_full", &f::co1_full),
        make_case("co1_min", &f::co1_min),
        make_case("co1_tsseq", &f::co1_tsseq),
        make_case("co1_payload", &f::co1_payload),
        make_case("fd_min", &f::fd_min),
        make_case("fd_report", &f::fd_report),
        make_case("fd_full", &f::fd_full),
        make_case("fd_ctrl", &f::fd_ctrl),
    };
    return cases;
}

/// Deterministic xorshift64* generator for the fuzz loops.
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ULL) {}
    uint64_t next() {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 2685821657736338717ULL;
    }
    uint64_t below(uint64_t n) { return next() % n; }
};

} // namespace e2sm_dapp_test

#endif // LIBE3_E2SM_DAPP_TEST_UTIL_HPP
