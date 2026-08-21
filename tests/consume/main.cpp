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
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include <libe3/version.hpp>

#include <cstdio>

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
    return 0;
}
