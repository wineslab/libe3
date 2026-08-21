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
    # Source side (A1, A2)
    0x10: "RECORD_BEGIN", 0x11: "PROCESS_BEGIN",
    # E3SM codec (A3, A9, A12, A18)
    0x18: "ENCODE_E3SM_BEGIN", 0x19: "ENCODE_E3SM_DONE",
    0x1A: "DECODE_E3SM_BEGIN", 0x1B: "DECODE_E3SM_DONE",
    # E3AP codec, queue and transport: libe3 (A4-A8, A13-A17, B10-B13)
    0x20: "EMIT_ENTER", 0x21: "ENQUEUE", 0x22: "DEQUEUE",
    0x23: "ENCODE_E3AP_DONE", 0x24: "SEND_DONE",
    0x25: "RECV", 0x26: "DECODE_E3AP_DONE",
    0x27: "DELIVER_BEGIN", 0x28: "DELIVER_DONE",
    0x29: "REPORT_QUEUED", 0x2A: "REPORT_DONE",
    0x2B: "SESSION_QUEUED", 0x2C: "SESSION_POLLED",
    0x2F: "DROP",
    # Bootstrap: setup, subscription, SM lifecycle
    0x30: "SETUP_BEGIN", 0x31: "SETUP_DONE",
    0x32: "SUB_BEGIN", 0x33: "SUB_DONE",
    0x34: "SM_START", 0x35: "SM_STOP",
    0x36: "SM_STATUS_BEGIN", 0x37: "SM_STATUS_DONE",
    # Receiving application, dApp side (A10, A11, B15)
    0x40: "PROCESS_DONE", 0x41: "CREATE_OUTPUT",
    0x42: "APPLY_POLICY_DONE", 0x43: "ADMITTED",
    # Receiving application, RAN side (A19a), and the acknowledgment tail
    0x48: "APPLY_CONTROL_DONE", 0x49: "LIVE_ON_AIR",
    0x4A: "ACK_SENT", 0x4B: "ACK_RECV",
    # E2-E3 bridge (A20, B9)
    0x50: "BRIDGE_IN", 0x51: "BRIDGE_OUT",
    # E2AP codec and SCTP transport (B1, B2, B6, B7)
    0x58: "ENCODE_E2AP_BEGIN", 0x59: "ENCODE_E2AP_DONE",
    0x5A: "DECODE_E2AP_BEGIN", 0x5B: "DECODE_E2AP_DONE",
    # E2SM codec and xApp decision (B3, B4, B5, B8)
    0x60: "DECODE_E2SM_BEGIN", 0x61: "DECODE_E2SM_DONE",
    0x62: "XAPP_PROCESS_DONE",
    0x63: "ENCODE_E2SM_BEGIN", 0x64: "ENCODE_E2SM_DONE",
    # Off-path: load and outcome records, not boxes
    0x70: "WAIT_ENTER", 0x71: "SKIPPED", 0x72: "CONTEXT",
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
