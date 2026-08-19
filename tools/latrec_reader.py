"""Reader for the latrec ring format written by include/libe3/latrec.h.

The format is a cross-repo contract: libe3, the C++ and Python dApps and flexric
all write it, and every one of those writers mirrors the layout in its own
source. This module is the reference reader, and lives next to the header so the
two move together -- tests/test_latrec_reader.py round-trips a ring produced by
the C writer through it, and checks the stage table below against the catalog in
latrec.h, so a new stage or a header change cannot land without updating both.

Standard library only, on purpose: pulling numpy in would put a build-time
dependency on a library that ships none. Callers that want bulk array work can
read the records and convert.

A ring is <role>.<tid>.latrec: a 4 KiB header followed by `entries` 32-byte
records in write order. Records are valid iff t_ns != 0 -- the writer stores
t_ns last with release ordering -- so a ring can be read while its writer is
still running, or after it crashed.

Usage:
    from latrec_reader import read_dir, STAGES
    for ring in read_dir("results/run_1"):
        print(ring.name, ring.rec_count, ring.wrapped)
        for r in ring.records:
            print(STAGES.get(r.stage), r.t_ns, r.aux)
"""
import glob
import os
import struct
from collections import namedtuple

LATREC_MAGIC = 0x31524C41  # "ALR1"
LATREC_VERSION = 2
HDR_LEN = 4096
REC_SIZE = 32

_HDR = struct.Struct("<IIQQQQ64s")  # v1 prefix; offsets frozen across versions
_HDR_V2 = struct.Struct("<IIQQ")    # rec_size, clock_ns_per_call, t1_mono, t1_real
_V2_OFF = 104
_REC = struct.Struct("<QQQQ")

# Stage ids, mirroring the catalog in include/libe3/latrec.h. Checked against it
# by tests/test_latrec_reader.py, so the two cannot drift apart.
STAGES = {
    0x10: "P0_PUSH_ENTRY", 0x11: "P1_COPY_DONE", 0x12: "P2_MASK_DONE",
    0x13: "P3_INFO_BUILT", 0x14: "P4_CHAN_LOCKED", 0x15: "P5_PUBLISHED",
    0x16: "P6_RX_ENTRY", 0x17: "P7_UESPEC_DONE", 0x18: "P8_UL_IND_DONE",
    0x19: "P9_RING_READY",
    0x20: "W0_WAKE", 0x21: "W1_SLOT_SELECT", 0x22: "W2_META_BUILT",
    0x23: "W3_ENCODE_DONE", 0x24: "W4_SENT_TO_E3", 0x25: "W5_WAIT_ENTER",
    0x26: "W6_SKIPPED",
    0x2B: "LS3_SUB_SENT", 0x2C: "LS2_SUB_RECV",
    0x2D: "LF1_CALLBACK_DONE", 0x2E: "LF0_FILTER_PASSED",
    0x2F: "LE0_EMIT_ENTER",
    0x30: "L0_ENQUEUE", 0x31: "L1_DEQUEUE", 0x32: "L2_ENCODE_DONE",
    0x33: "L3_SEND_DONE", 0x34: "L4_RECV", 0x35: "L5_DECODED",
    0x36: "L6_DISPATCHED", 0x37: "L7_REPORT_QUEUED", 0x38: "L8_REPORT_DONE",
    0x39: "L9_DROP", 0x3A: "LS0_SETUP_RECV", 0x3B: "LS1_SETUP_SENT",
    0x3C: "LQ0_QUEUED", 0x3D: "LQ1_POLLED",
    0x3E: "LC0_SEND_ENTER", 0x3F: "LC1_SEND_RETURNED",
    0x40: "D0_RECV", 0x41: "D1_PARSED", 0x42: "D2_DISPATCHED",
    0x43: "D3_HANDLER_IN", 0x44: "D4_RX_ACCOUNTED", 0x45: "D5_ADMITTED",
    0x46: "D6_COMPUTED", 0x47: "D7_DETECTED", 0x48: "D8_SM_SENT",
    0x49: "D9_SNAPPED", 0x4A: "D10_HANDLER_OUT", 0x4B: "D11_L2SCAN_DONE",
    0x4C: "D12_ENCODE_DONE", 0x4D: "D13_SENSE_IN", 0x4E: "D14_SENSE_OUT",
    0x4F: "D15_CONVERT_DONE", 0x56: "D16_DETECT_PRE",
    0x5A: "D17_GPU_SUBMIT", 0x5B: "D18_GPU_RETIRED", 0x5C: "D19_VIZ_SENT",
    0x50: "V0_SNAP_TAKEN", 0x51: "V1_QUANTIZED", 0x52: "V2_PUBLISHED",
    0x53: "V3_WOKE",
    0x54: "E0_SUB_SENT", 0x55: "E1_SUB_CONFIRMED", 0x57: "E2_SETUP_SENT",
    0x58: "E3_SETUP_RESP", 0x59: "E4_SETUP_READY",
    0x60: "C0_CONTEXT",
    0x61: "M0_SLOT_ENTRY", 0x62: "M1_LOCK_HELD", 0x63: "M2_BLOCK_APPLIED",
    0x64: "M3_UL_DONE", 0x65: "M4_DL_DONE", 0x66: "M5_PUCCH_DONE",
    0x67: "M6_SENSING_DONE", 0x68: "M7_SLOT_EXIT",
    0x69: "T0_SM_START", 0x6A: "T1_SM_STOP", 0x6B: "T2_STATUS_IN",
    0x6C: "T3_PERIOD_SET", 0x6D: "T4_RIC_UPDATED", 0x6E: "T5_STATUS_DONE",
    0x70: "S0_RECORD_IN", 0x71: "S1_PUBLISHED", 0x72: "S2_WORKER_WAKE",
    0x73: "S3_RANGES_READ", 0x74: "S4_SHM_WRITTEN", 0x75: "S5_ENCODE_DONE",
    0x76: "S6_SENT_TO_E3", 0x77: "S7_WAIT_ENTER", 0x78: "S8_SKIPPED",
    0x80: "B0_CTRL_RECV", 0x81: "B1_DECODED", 0x82: "B2_PREPARED",
    0x83: "B3_UL_INSTALLED", 0x84: "B4_INSTALLED", 0x85: "B5_ACKED",
    0x86: "B6_LIVE_ON_AIR",
    0x90: "G0_CTRL_RECV", 0x91: "G1_CTRL_E3_SENT",
    0x98: "G8_REP_RECV", 0x99: "G9_REP_TO_E2",
    0xA0: "K0_CTRL_RECV", 0xA1: "K1_CTRL_APPLIED", 0xA2: "K2_CTRL_ACKED",
    0xA3: "K3_CTRL_SENT", 0xA4: "K4_ACK_RECV", 0xA5: "K5_CTRL_DECODED",
    0xA6: "K6_PUBLISHED",
    0xA8: "R0_REP_BUILT", 0xA9: "R1_REP_SENT",
    0xB0: "X0_CTRL_REQ", 0xB1: "X1_CTRL_SM_ENC", 0xB2: "X2_CTRL_E2AP_ENC",
    0xB3: "X3_CTRL_SCTP_SENT", 0xB4: "X4_AG_E2AP_DEC", 0xB5: "X5_AG_CTRL_IN",
    0xB6: "X6_AG_SM_DEC", 0xB7: "X7_AG_CTRL_OUT", 0xB8: "X8_AG_IND_ENC",
    0xB9: "X9_AG_IND_E2AP", 0xBA: "XA_AG_IND_SENT", 0xBB: "XB_IND_RECV",
    0xBC: "XC_IND_DEC", 0xBD: "XD_IND_DISPATCH", 0xBE: "XE_REP_RECV",
    # Reserved: not yet stamped by libe3 itself (see include/libe3/latrec.h).
    0xC0: "OC0_CAPTURE_ENTRY", 0xC1: "OC1_CAPTURE_DONE",
    0xC2: "OC2_PIPELINE_WAKE", 0xC3: "OC3_PIPELINE_DONE",
    0xC4: "OC4_ENCODE_DONE",
    0xD0: "CB0_LAKE_READ_ENTRY", 0xD1: "CB1_LAKE_READ_DONE",
    0xD2: "CB2_AGENT_WAKE", 0xD3: "CB3_AGENT_DONE", 0xD4: "CB4_ENCODE_DONE",
    0xE0: "PY0_IND_DISPATCH", 0xE1: "PY1_SM_DECODED", 0xE2: "PY2_XAPP_IN",
    0xE3: "PY3_XAPP_OUT", 0xE4: "PY4_SM_ENCODED",
    # Stamped by this repo's examples/benchmark only.
    0xF0: "EX0_COLLECT_BEGIN", 0xF1: "EX1_ENCODE_BEGIN",
    0xF2: "EX2_SEND_INDICATION", 0xF3: "EX3_CTRL_RECV", 0xF4: "EX4_CTRL_DONE",
    0xF6: "BD1_DECODED", 0xF7: "BD2_CTRL_ENCODE_BEGIN",
    0xF8: "BD3_CTRL_ENCODE_DONE",
}

Record = namedtuple("Record", "seq cpu stage t_ns aux aux2")


class BadRing(Exception):
    """The file is not a latrec ring (bad magic, truncated, or byte-swapped)."""


class Ring:
    """One ring file: header fields plus its valid records in write order.

    wrapped/lost_records are the capture's honesty check. A writer never stops
    to tell you it overran; rec_count keeps counting past `entries`, so
    lost_records is how many records the ring dropped. rec_count == 0 means the
    process never ran its exit flush (it was killed) -- the records are still
    valid, but the write count is unknown.
    """

    __slots__ = ("file", "name", "version", "entries", "rec_count", "rec_size",
                 "clock_ns_per_call", "t0_mono_ns", "t0_real_ns", "t1_mono_ns",
                 "t1_real_ns", "records", "wrapped", "descents")

    @property
    def lost_records(self):
        return max(0, self.rec_count - self.entries)

    @property
    def valid(self):
        return len(self.records)

    @property
    def bytes(self):
        return HDR_LEN + self.entries * REC_SIZE

    def mono_to_real_ns(self, t_ns):
        """Map a record's CLOCK_MONOTONIC stamp onto CLOCK_REALTIME.

        Records are stamped with CLOCK_MONOTONIC: within one host that is what
        you want, since it cannot be stepped. Across hosts the origins are
        unrelated -- monotonic counts from boot -- so joining records from two
        machines needs the wall clock, which PTP or NTP is what actually
        synchronises.

        The writer stores a paired (mono, real) reading at open and refreshes it
        at exit, so the mapping is a two-point fit: it absorbs any rate skew
        between the two clocks over the run, and degrades to a constant offset
        when only the opening pair is present (an unflushed ring).

        The result is only as good as the hosts' wall-clock sync, and a
        cross-host hop smaller than that error is not measurable.

        The mapping is per ring, so a hop whose two ends are stamped by
        different threads picks up the disagreement between their fits. Within
        one host the raw monotonic stamps avoid that entirely.
        """
        if not self.t0_real_ns:
            raise BadRing(f"{self.file}: no clock pair; cannot map to realtime")
        # Integer arithmetic throughout: a wall-clock nanosecond is ~1.8e18, and
        # float64 resolves that only to ~256 ns, which would quantise a hop
        # measured in microseconds.
        span = self.t1_mono_ns - self.t0_mono_ns
        if self.t1_real_ns and span > 0:
            return (self.t0_real_ns
                    + (t_ns - self.t0_mono_ns) * (self.t1_real_ns - self.t0_real_ns) // span)
        return t_ns + (self.t0_real_ns - self.t0_mono_ns)

    @property
    def mono_real_offset_ns(self):
        """Constant part of the mapping, for auditing a cross-host capture."""
        return self.t0_real_ns - self.t0_mono_ns if self.t0_real_ns else 0


def read_ring(path):
    """Parse one .latrec file. Raises BadRing if it is not one."""
    with open(path, "rb") as f:
        hdr = f.read(HDR_LEN)
        if len(hdr) < HDR_LEN:
            raise BadRing(f"{path}: shorter than a header")
        magic, ver, entries, rec_count, t0m, t0r, name = _HDR.unpack_from(hdr, 0)
        if magic != LATREC_MAGIC:
            # A byte-swapped magic means the ring came from an opposite-endian
            # host; the format is little-endian only, so say so precisely.
            if struct.unpack("<I", struct.pack(">I", magic))[0] == LATREC_MAGIC:
                raise BadRing(f"{path}: byte-swapped ring (opposite-endian writer)")
            raise BadRing(f"{path}: bad magic 0x{magic:08X}")
        if ver >= 2:
            rec_size, clock_ns, t1m, t1r = _HDR_V2.unpack_from(hdr, _V2_OFF)
        else:
            rec_size, clock_ns, t1m, t1r = REC_SIZE, 0, 0, 0
        if rec_size not in (0, REC_SIZE):
            raise BadRing(f"{path}: rec_size {rec_size}, expected {REC_SIZE}")
        blob = f.read(entries * REC_SIZE)

    recs = []
    for sc, t_ns, aux, aux2 in _REC.iter_unpack(blob[:len(blob) // REC_SIZE * REC_SIZE]):
        if t_ns:  # released last: nonzero == the rest of the record is visible
            recs.append(Record(sc & 0x0000FFFFFFFFFFFF, (sc >> 48) & 0xFF,
                               sc >> 56, t_ns, aux, aux2))

    # One writer stamps a ring in time order, so t_ns descends only where the
    # ring wrapped -- exactly once. Rotate there to recover write order. More
    # than one descent means it is not a single-writer ring; report it rather
    # than rotating at an arbitrary point.
    descents = [i for i in range(1, len(recs)) if recs[i].t_ns < recs[i - 1].t_ns]
    wrapped = False
    if len(descents) == 1:
        cut = descents[0]
        recs = recs[cut:] + recs[:cut]
        wrapped = True

    r = Ring()
    r.file, r.name = os.path.basename(path), name.rstrip(b"\0").decode()
    r.version, r.entries, r.rec_count = ver, entries, rec_count
    r.rec_size, r.clock_ns_per_call = rec_size, clock_ns
    r.t0_mono_ns, r.t0_real_ns, r.t1_mono_ns, r.t1_real_ns = t0m, t0r, t1m, t1r
    r.records, r.wrapped, r.descents = recs, wrapped, len(descents)
    return r


def read_dir(path, on_bad=None):
    """Parse every *.latrec in a directory, sorted by name.

    on_bad(path, exc) is called for files that are not rings; the default drops
    them silently so a partially written directory still converts.
    """
    out = []
    for p in sorted(glob.glob(os.path.join(path, "*.latrec"))):
        try:
            out.append(read_ring(p))
        except BadRing as e:
            if on_bad:
                on_bad(p, e)
    return out
