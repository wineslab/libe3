/**
 * @file main.cpp
 * @brief Compile-and-link check for the installed libe3 package.
 *
 * Includes the umbrella header (which reaches e3_encoder.hpp, and through it
 * <tl/expected.hpp>) and touches enough of the API to force a link. The
 * feature macros are checked at compile time: whichever encodings the install
 * was built with must be visible here, otherwise the consumer and the library
 * disagree about the shape of the headers.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include <libe3/version.hpp>

#include <cstdio>
#include <vector>

#ifdef CONSUME_HAVE_E2SM_DAPP
#include <libe3/e2sm_dapp.hpp>
#ifndef LIBE3_ENABLE_E2SM_DAPP
#error "LIBE3_ENABLE_E2SM_DAPP is not defined: the package did not export its feature macro"
#endif
#endif

#if !defined(LIBE3_ENABLE_ASN1) && !defined(LIBE3_ENABLE_JSON) && !defined(LIBE3_ENABLE_PROTOBUF)
#error "no libe3 encoding macro is defined: the package did not export its feature macros"
#endif

#if !defined(LIBE3_HAS_ZMQ)
#error "LIBE3_HAS_ZMQ is not defined: the package did not export its feature macros"
#endif

int main() {
    libe3::E3Config cfg;
    cfg.ran_identifier = "consume-check";
    libe3::E3Agent agent(std::move(cfg));
    std::printf("libe3 %s, agent state %d\n",
                LIBE3_VERSION_STRING, static_cast<int>(agent.state()));

#ifdef CONSUME_HAVE_E2SM_DAPP
    // The codec next to the agent: two asn1c runtimes in one executable.
    libe3::e2sm_dapp::e2sm_dapp_ind_hdr_s hdr;
    auto& f = hdr.ric_ind_hdr_formats.set_ind_hdr_format1();
    f.ran_function_id = 255;
    f.dapp_id = 7;
    f.node_nb_id = 1234;
    std::vector<uint8_t> bytes;
    libe3::e2sm_dapp::e2sm_dapp_ind_hdr_s back;
    if (hdr.pack(bytes) != libe3::e2sm_dapp::ASN_SUCCESS || bytes.empty() ||
        back.unpack(bytes.data(), bytes.size()) != libe3::e2sm_dapp::ASN_SUCCESS || !(back == hdr)) {
        std::printf("E2SM-DAPP codec round trip failed\n");
        return 1;
    }
    std::printf("E2SM-DAPP indication header: %zu bytes, round trip ok\n", bytes.size());
#endif
    return 0;
}
