/**
 * @file latrec_ring_reader.hpp
 * @brief Reads .latrec rings back, the way the offline tooling does.
 *
 * Shared by the latrec tests and benchmarks, so the format is parsed in one
 * place. The header is read through the frozen v1 byte offsets rather than by
 * casting to latrec_hdr, which is how an external reader sees the file.
 *
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

/** Every valid record from every ring in a directory, merged. */
inline std::vector<latrec_rec> read_ring_dir(const std::string& dir,
                                             bool* any_wrapped = nullptr) {
    std::vector<latrec_rec> out;
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    while (dirent* e = readdir(d)) {
        const std::string n = e->d_name;
        if (n.size() < 8 || n.compare(n.size() - 7, 7, ".latrec") != 0) continue;
        RingFile rf(dir + "/" + n);
        if (!rf.ok) continue;
        if (any_wrapped && rf.wrapped()) *any_wrapped = true;
        out.insert(out.end(), rf.valid.begin(), rf.valid.end());
    }
    closedir(d);
    return out;
}

}  // namespace latrec_test

#endif  // LIBE3_TESTS_LATREC_RING_READER_HPP
