/**
 * @file latrec_ring_reader.hpp
 * @brief Reads .latrec rings back, the way the offline tooling does.
 *
 * Shared by the latrec tests and benchmarks, so the format is parsed in one
 * place. The header is read through the frozen v1 byte offsets rather than by
 * casting to latrec_hdr, which is how an external reader sees the file.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIBE3_TESTS_LATREC_RING_READER_HPP
#define LIBE3_TESTS_LATREC_RING_READER_HPP

#include <libe3/latrec.h>

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

namespace latrec_test {

inline uint64_t seq_of(const latrec_rec& r)   { return r.sc & 0x0000FFFFFFFFFFFFull; }
inline uint8_t  stage_of(const latrec_rec& r) { return static_cast<uint8_t>(r.sc >> 56); }
inline uint8_t  cpu_of(const latrec_rec& r)   { return static_cast<uint8_t>(r.sc >> 48); }

/** One ring file: header fields plus the records that are valid (t_ns != 0). */
struct RingFile {
    bool ok{false};
    uint32_t magic{0}, version{0}, rec_size{0}, clock_ns{0};
    uint64_t entries{0}, rec_count{0}, t0_mono{0}, t0_real{0}, t1_mono{0}, t1_real{0};
    std::string name;
    std::vector<latrec_rec> valid;

    RingFile() = default;

    explicit RingFile(const std::string& path) {
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return;
        unsigned char hdr[LATREC_HDR_LEN];
        if (fread(hdr, 1, LATREC_HDR_LEN, f) != LATREC_HDR_LEN) { fclose(f); return; }
        std::memcpy(&magic,     hdr +   0, 4);
        std::memcpy(&version,   hdr +   4, 4);
        std::memcpy(&entries,   hdr +   8, 8);
        std::memcpy(&rec_count, hdr +  16, 8);
        std::memcpy(&t0_mono,   hdr +  24, 8);
        std::memcpy(&t0_real,   hdr +  32, 8);
        name.assign(reinterpret_cast<const char*>(hdr + 40));
        std::memcpy(&rec_size,  hdr + 104, 4);   // v2, appended inside the pad
        std::memcpy(&clock_ns,  hdr + 108, 4);
        std::memcpy(&t1_mono,   hdr + 112, 8);
        std::memcpy(&t1_real,   hdr + 120, 8);
        latrec_rec r;
        while (fread(&r, sizeof(r), 1, f) == 1) {
            if (r.t_ns) valid.push_back(r);
        }
        fclose(f);
        ok = (magic == LATREC_MAGIC);
    }

    /** More written than the ring holds: the oldest records were overwritten. */
    bool wrapped() const { return rec_count > entries; }
};

/** A record together with the ring that wrote it. The stage catalog names
 *  operations, not components, so the ring is what says which side of the loop
 *  a record came from -- both ends of a round trip encode a Service Model
 *  payload under the same identifier. */
struct Stamped {
    std::string ring;      /**< the ring's name, "<role>.<tid>" */
    latrec_rec  rec;
};

/** True when `ring` was opened with a role of exactly `role`, ignoring the tid
 *  suffix. */
inline bool ring_role_is(const std::string& ring, const std::string& role) {
    return ring.size() > role.size() + 1 && ring.compare(0, role.size(), role) == 0
        && ring[role.size()] == '.';
}

/** Every valid record from every ring in a directory, tagged with its ring. */
inline std::vector<Stamped> read_ring_dir_tagged(const std::string& dir,
                                                 bool* any_wrapped = nullptr) {
    std::vector<Stamped> out;
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    while (dirent* e = readdir(d)) {
        const std::string n = e->d_name;
        if (n.size() < 8 || n.compare(n.size() - 7, 7, ".latrec") != 0) continue;
        RingFile rf(dir + "/" + n);
        if (!rf.ok) continue;
        if (any_wrapped && rf.wrapped()) *any_wrapped = true;
        for (const auto& r : rf.valid) out.push_back({rf.name, r});
    }
    closedir(d);
    return out;
}

/** Every valid record from every ring in a directory, merged, ring discarded. */
inline std::vector<latrec_rec> read_ring_dir(const std::string& dir,
                                             bool* any_wrapped = nullptr) {
    std::vector<latrec_rec> out;
    for (const auto& s : read_ring_dir_tagged(dir, any_wrapped)) out.push_back(s.rec);
    return out;
}

}  // namespace latrec_test

#endif  // LIBE3_TESTS_LATREC_RING_READER_HPP
