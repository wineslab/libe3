/**
 * @file e2sm_dapp.cpp
 * @brief Compile-and-link check for the installed E2SM-DAPP codec, C++ API.
 *
 * Links libe3::e2sm_dapp and nothing else of libe3: the codec has to work
 * without the agent, ZeroMQ or the E3AP grammar. Packs the same event trigger
 * and indication header as the golden vectors and checks the bytes.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/e2sm_dapp.hpp>

#include <cstdio>
#include <vector>

#ifndef LIBE3_ENABLE_E2SM_DAPP
#error "LIBE3_ENABLE_E2SM_DAPP is not defined: the package did not export its feature macro"
#endif

extern "C" int consume_e2sm_dapp_c(void);

int main() {
    using namespace libe3::e2sm_dapp;

    std::vector<uint8_t> bytes;
    e2sm_dapp_event_trigger_s et;
    if (et.pack(bytes) != ASN_SUCCESS || bytes != std::vector<uint8_t>{0x00}) {
        std::printf("event trigger: unexpected encoding\n");
        return 1;
    }

    // ih1_min of the golden vectors: 00 00 ff 00 01 00 00 13 00 14 00 00.
    e2sm_dapp_ind_hdr_s hdr;
    auto& f = hdr.ric_ind_hdr_formats.set_ind_hdr_format1();
    f.ran_function_id = 255;
    f.dapp_id         = 1;
    f.node_type       = 0;
    f.node_plmn_id[0] = 0x13;
    f.node_plmn_id[1] = 0x00;
    f.node_plmn_id[2] = 0x14;
    const std::vector<uint8_t> golden = {0x00, 0x00, 0xff, 0x00, 0x01, 0x00, 0x00, 0x13, 0x00, 0x14, 0x00, 0x00};
    if (hdr.pack(bytes) != ASN_SUCCESS || bytes != golden) {
        std::printf("indication header: unexpected encoding\n");
        return 1;
    }
    e2sm_dapp_ind_hdr_s back;
    if (back.unpack(bytes.data(), bytes.size()) != ASN_SUCCESS || !(back == hdr)) {
        std::printf("indication header: round trip failed\n");
        return 1;
    }
    // An oversized payload is refused, not a crash.
    e2sm_dapp_ctrl_msg_s msg;
    msg.ric_ctrl_msg_formats.ctrl_msg_format1().data.resize(40000);
    if (msg.pack(bytes) != ASN_ERROR_ENCODE_FAIL || !bytes.empty()) {
        std::printf("control message: oversized payload not refused\n");
        return 1;
    }
    std::printf("e2sm_dapp C++ ok\n");
    return consume_e2sm_dapp_c();
}
