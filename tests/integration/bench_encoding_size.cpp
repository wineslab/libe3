/**
 * @file bench_encoding_size.cpp
 * @brief Measure the wire size (bytes) and encode/decode CPU cost of each
 *        E3AP PDU type under every enabled encoding (ASN.1 APER, JSON;
 *        protobuf when available).
 *
 * Timing uses Google Benchmark: iteration counts are auto-calibrated so
 * sub-microsecond encode/decode operations are batched (amortizing the
 * clock-read overhead), results are guarded with benchmark::DoNotOptimize
 * to defeat dead-code elimination, and p50/p99 are computed across
 * repetitions. Pin the process (e.g. `taskset -c 2`) and use the
 * `performance` CPU governor for stable numbers; Google Benchmark warns on
 * stderr when CPU frequency scaling is active.
 *
 * Outputs a CSV to stdout with columns:
 *   encoding, message_type, encoded_bytes, info_bytes,
 *   encode_ns_p50, encode_ns_p99, decode_ns_p50, decode_ns_p99
 *
 * `info_bytes` is the intrinsic information content of the sample PDU (sum
 * of the raw sizes of the fields it carries), so encoded_bytes/info_bytes
 * reads as the framing overhead of each encoding.
 *
 * Usage:
 *   ./bench_encoding_size
 *   taskset -c 2 ./bench_encoding_size | python scripts/collect_baseline.py \
 *       --type encoding --output data/baseline/encoding/encoding.csv
 *
 * Representative values are used for each field; opaque payload fields
 * (protocol_data, action_data, etc.) use a 64-byte pattern so that the
 * numbers reflect realistic in-field usage without being dominated by
 * the payload itself.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include <libe3/e3_encoder.hpp>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace libe3;

namespace {

constexpr int kRepetitions = 20;
constexpr double kMinTimeSec = 0.02;

// ---------------------------------------------------------------------------
// Sample PDUs
// ---------------------------------------------------------------------------

// 64-byte representative payload for opaque binary fields (ASN.1 path).
std::vector<uint8_t> small_payload() {
    std::vector<uint8_t> v(64);
    for (size_t i = 0; i < v.size(); ++i) v[i] = static_cast<uint8_t>(i);
    return v;
}

// JSON-formatted payload for IndicationMessage.protocol_data when encoding==JSON.
// The JSON encoder embeds protocol_data as a nested JSON object (not hex).
// This is the canonical Simple SM indication payload produced by the example
// wrapper's JSON branch.
std::vector<uint8_t> json_sm_payload() {
    const char* s = R"({"data1":42,"timestamp":1234567890})";
    return std::vector<uint8_t>(s, s + std::strlen(s));
}

// One sample PDU plus its intrinsic information content in bytes.
struct Sample {
    const char* name;
    Pdu pdu;
    size_t info_bytes;
    Sample(const char* n, PduType t) : name(n), pdu(t), info_bytes(0) {}
};

constexpr size_t kU32 = sizeof(uint32_t);
constexpr size_t kEnum = 1;  // ResponseCode fits one byte of information
constexpr size_t kMsgId = kU32;

// Build the 11 sample PDUs. json_indication: when true, use a JSON-formatted
// indication payload instead of raw bytes (the JSON encoder embeds
// protocol_data as a nested JSON object).
std::vector<Sample> make_samples(bool json_indication) {
    std::vector<Sample> samples;
    auto payload = small_payload();
    auto ind_payload = json_indication ? json_sm_payload() : payload;

    // 1. SetupRequest
    {
        Sample s("SetupRequest", PduType::SETUP_REQUEST);
        SetupRequest req;
        req.e3ap_protocol_version = "1.0.0";
        req.dapp_name             = "BenchDApp";
        req.dapp_version          = "1.0.0";
        req.vendor                = "WinesLab";
        s.pdu.choice = req;
        s.pdu.message_id = 1;
        s.info_bytes = kMsgId + req.e3ap_protocol_version.size() +
                       req.dapp_name.size() + req.dapp_version.size() +
                       req.vendor.size();
        samples.push_back(std::move(s));
    }

    // 2. SetupResponse
    {
        Sample s("SetupResponse", PduType::SETUP_RESPONSE);
        SetupResponse resp;
        resp.request_id            = 1;
        resp.response_code         = ResponseCode::POSITIVE;
        resp.ran_identifier        = "bench-ran-001";
        resp.e3ap_protocol_version = "1.0.0";
        resp.dapp_identifier       = 1;
        RanFunctionDef rfdef;
        rfdef.ran_function_identifier = 1;
        rfdef.telemetry_identifier_list = {1};
        rfdef.control_identifier_list   = {1};
        rfdef.ran_function_data = payload;
        resp.ran_function_list.push_back(rfdef);
        s.pdu.choice = resp;
        s.pdu.message_id = 2;
        s.info_bytes = kMsgId + kU32 + kEnum + resp.ran_identifier.size() +
                       (resp.e3ap_protocol_version ? resp.e3ap_protocol_version->size() : 0) + kU32 +
                       (kU32 + kU32 * rfdef.telemetry_identifier_list.size() +
                        kU32 * rfdef.control_identifier_list.size() +
                        rfdef.ran_function_data.size());
        samples.push_back(std::move(s));
    }

    // 3. SubscriptionRequest
    {
        Sample s("SubscriptionRequest", PduType::SUBSCRIPTION_REQUEST);
        SubscriptionRequest req;
        req.dapp_identifier           = 1;
        req.ran_function_identifier   = 1;
        req.telemetry_identifier_list = {1};
        req.control_identifier_list   = {1};
        req.subscription_time         = 0;
        req.periodicity               = 1000;
        s.pdu.choice = req;
        s.pdu.message_id = 3;
        s.info_bytes = kMsgId + kU32 + kU32 +
                       kU32 * req.telemetry_identifier_list.size() +
                       kU32 * req.control_identifier_list.size() +
                       kU32 + kU32;
        samples.push_back(std::move(s));
    }

    // 4. SubscriptionDelete
    {
        Sample s("SubscriptionDelete", PduType::SUBSCRIPTION_DELETE);
        SubscriptionDelete del;
        del.dapp_identifier  = 1;
        del.subscription_id  = 1;
        s.pdu.choice = del;
        s.pdu.message_id = 4;
        s.info_bytes = kMsgId + kU32 + kU32;
        samples.push_back(std::move(s));
    }

    // 5. SubscriptionResponse
    {
        Sample s("SubscriptionResponse", PduType::SUBSCRIPTION_RESPONSE);
        SubscriptionResponse resp;
        resp.request_id       = 3;
        resp.dapp_identifier  = 1;
        resp.response_code    = ResponseCode::POSITIVE;
        resp.subscription_id  = 1;
        s.pdu.choice = resp;
        s.pdu.message_id = 5;
        s.info_bytes = kMsgId + kU32 + kU32 + kEnum + kU32;
        samples.push_back(std::move(s));
    }

    // 6. IndicationMessage
    {
        Sample s("IndicationMessage", PduType::INDICATION_MESSAGE);
        IndicationMessage msg;
        msg.dapp_identifier        = 1;
        msg.ran_function_identifier = 1;
        msg.protocol_data          = ind_payload;
        s.pdu.choice = msg;
        s.pdu.message_id = 6;
        s.info_bytes = kMsgId + kU32 + kU32 + msg.protocol_data.size();
        samples.push_back(std::move(s));
    }

    // 7. DAppControlAction
    {
        Sample s("DAppControlAction", PduType::DAPP_CONTROL_ACTION);
        DAppControlAction action;
        action.dapp_identifier        = 1;
        action.ran_function_identifier = 1;
        action.control_identifier     = 1;
        action.action_data            = payload;
        s.pdu.choice = action;
        s.pdu.message_id = 7;
        s.info_bytes = kMsgId + kU32 + kU32 + kU32 + action.action_data.size();
        samples.push_back(std::move(s));
    }

    // 8. DAppReport
    {
        Sample s("DAppReport", PduType::DAPP_REPORT);
        DAppReport rep;
        rep.dapp_identifier        = 1;
        rep.ran_function_identifier = 1;
        rep.report_data            = payload;
        s.pdu.choice = rep;
        s.pdu.message_id = 8;
        s.info_bytes = kMsgId + kU32 + kU32 + rep.report_data.size();
        samples.push_back(std::move(s));
    }

    // 9. XAppControlAction
    {
        Sample s("XAppControlAction", PduType::XAPP_CONTROL_ACTION);
        XAppControlAction xaction;
        xaction.dapp_identifier        = 1;
        xaction.ran_function_identifier = 1;
        xaction.xapp_control_data      = payload;
        s.pdu.choice = xaction;
        s.pdu.message_id = 9;
        s.info_bytes = kMsgId + kU32 + kU32 + xaction.xapp_control_data.size();
        samples.push_back(std::move(s));
    }

    // 10. ReleaseMessage
    {
        Sample s("ReleaseMessage", PduType::RELEASE_MESSAGE);
        ReleaseMessage rel;
        rel.dapp_identifier = 1;
        s.pdu.choice = rel;
        s.pdu.message_id = 10;
        s.info_bytes = kMsgId + kU32;
        samples.push_back(std::move(s));
    }

    // 11. MessageAck
    {
        Sample s("MessageAck", PduType::MESSAGE_ACK);
        MessageAck ack;
        ack.request_id    = 1;
        ack.response_code = ResponseCode::POSITIVE;
        s.pdu.choice = ack;
        s.pdu.message_id = 11;
        s.info_bytes = kMsgId + kU32 + kEnum;
        samples.push_back(std::move(s));
    }

    return samples;
}

// ---------------------------------------------------------------------------
// Google Benchmark plumbing
// ---------------------------------------------------------------------------

double stat_percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t idx = static_cast<size_t>(p * static_cast<double>(v.size() - 1));
    return v[idx];
}

double stat_p50(const std::vector<double>& v) { return stat_percentile(v, 0.50); }
double stat_p99(const std::vector<double>& v) { return stat_percentile(v, 0.99); }

// One (encoding, PDU) measurement cell: pre-encoded buffer for the decode
// direction plus the CSV size columns.
struct Cell {
    std::string encoding;      // CSV encoding name (asn1/json/protobuf)
    std::string message;       // PDU type name
    size_t encoded_bytes = 0;
    size_t info_bytes = 0;
    E3Encoder* enc = nullptr;
    Pdu pdu{PduType::MESSAGE_ACK};
    EncodedMessage encoded;
};

// Captures the p50/p99 aggregate rows emitted by Google Benchmark instead of
// printing a console report, so the process output stays pure CSV.
class CaptureReporter : public benchmark::BenchmarkReporter {
public:
    bool ReportContext(const Context&) override { return true; }
    void ReportRuns(const std::vector<Run>& runs) override {
        for (const auto& run : runs) {
            if (run.skipped) continue;
            if (run.run_type != Run::RT_Aggregate) continue;
            if (run.aggregate_name != "p50" && run.aggregate_name != "p99") continue;
            results_[run.run_name.function_name][run.aggregate_name] =
                run.GetAdjustedRealTime();
        }
    }

    double get(const std::string& bench, const std::string& stat) const {
        auto it = results_.find(bench);
        if (it == results_.end()) return 0.0;
        auto jt = it->second.find(stat);
        return jt == it->second.end() ? 0.0 : jt->second;
    }

private:
    std::map<std::string, std::map<std::string, double>> results_;
};

void register_cell(Cell& cell) {
    E3Encoder* enc = cell.enc;
    const Pdu* pdu = &cell.pdu;
    const EncodedMessage* encoded = &cell.encoded;

    auto* b_enc = benchmark::RegisterBenchmark(
        ("encode/" + cell.encoding + "/" + cell.message).c_str(),
        [enc, pdu](benchmark::State& state) {
            for (auto _ : state) {
                auto result = enc->encode(*pdu);
                benchmark::DoNotOptimize(result);
            }
        });
    auto* b_dec = benchmark::RegisterBenchmark(
        ("decode/" + cell.encoding + "/" + cell.message).c_str(),
        [enc, encoded](benchmark::State& state) {
            for (auto _ : state) {
                auto result = enc->decode(*encoded);
                benchmark::DoNotOptimize(result);
            }
        });
    for (auto* b : {b_enc, b_dec}) {
        b->Unit(benchmark::kNanosecond)
            ->MinTime(kMinTimeSec)
            ->Repetitions(kRepetitions)
            ->ComputeStatistics("p50", stat_p50)
            ->ComputeStatistics("p99", stat_p99)
            ->ReportAggregatesOnly(true);
    }
}

}  // namespace

// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    benchmark::Initialize(&argc, argv);

    struct EncoderCase {
        EncodingFormat format;
        const char* name;
        bool json_indication;
    };
    std::vector<EncoderCase> encoder_cases;
#ifdef LIBE3_ENABLE_ASN1
    encoder_cases.push_back({EncodingFormat::ASN1, "asn1", false});
#endif
#ifdef LIBE3_ENABLE_JSON
    encoder_cases.push_back({EncodingFormat::JSON, "json", true});
#endif
#ifdef LIBE3_ENABLE_PROTOBUF
    encoder_cases.push_back({EncodingFormat::PROTOBUF, "protobuf", false});
#endif

    std::vector<std::unique_ptr<E3Encoder>> encoders;
    std::vector<Cell> cells;
    for (const auto& ec : encoder_cases) {
        auto enc = create_encoder(ec.format);
        if (!enc) {
            std::fprintf(stderr, "WARN: no encoder for %s\n", ec.name);
            continue;
        }
        for (auto& sample : make_samples(ec.json_indication)) {
            auto result = enc->encode(sample.pdu);
            if (!result.has_value()) {
                std::fprintf(stderr, "WARN: encode failed for %s/%s\n",
                             ec.name, sample.name);
                continue;
            }
            Cell cell;
            cell.encoding      = ec.name;
            cell.message       = sample.name;
            cell.encoded_bytes = result->buffer.size();
            cell.info_bytes    = sample.info_bytes;
            cell.enc           = enc.get();
            cell.pdu           = sample.pdu;
            cell.encoded       = *result;
            cells.push_back(std::move(cell));
        }
        encoders.push_back(std::move(enc));
    }

    // Register after `cells` is fully built: the benchmark lambdas capture
    // pointers into the vector, which must not reallocate afterwards.
    for (auto& cell : cells) register_cell(cell);

    CaptureReporter capture;
    benchmark::RunSpecifiedBenchmarks(&capture);

    std::printf("encoding,message_type,encoded_bytes,info_bytes,"
                "encode_ns_p50,encode_ns_p99,decode_ns_p50,decode_ns_p99\n");
    for (const auto& cell : cells) {
        const std::string enc_key = "encode/" + cell.encoding + "/" + cell.message;
        const std::string dec_key = "decode/" + cell.encoding + "/" + cell.message;
        std::printf("%s,%s,%zu,%zu,%.1f,%.1f,%.1f,%.1f\n",
                    cell.encoding.c_str(), cell.message.c_str(),
                    cell.encoded_bytes, cell.info_bytes,
                    capture.get(enc_key, "p50"), capture.get(enc_key, "p99"),
                    capture.get(dec_key, "p50"), capture.get(dec_key, "p99"));
    }

    benchmark::Shutdown();
    return 0;
}
