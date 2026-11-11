/**
 * @file uring_ring.cpp
 * @brief One io_uring instance: the only code of libe3 that includes <liburing.h>
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "uring_ring.hpp"

#include <liburing.h>
#include <sys/uio.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <vector>

namespace libe3 {

namespace {

int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline void cpu_relax() {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
    asm volatile("yield" ::: "memory");
#endif
}

/// What the connector needs. All are older than IORING_FEAT_EXT_ARG (5.11), which is required too.
constexpr int REQUIRED_OPS[] = {
    IORING_OP_ACCEPT, IORING_OP_RECV, IORING_OP_SEND, IORING_OP_RECVMSG, IORING_OP_SENDMSG,
    IORING_OP_READ, IORING_OP_ASYNC_CANCEL, IORING_OP_READ_FIXED,
};

}  // namespace

/** @brief The liburing state, kept out of the header. */
struct UringRing::State {
    struct io_uring ring{};      ///< The ring
    bool initialised{false};     ///< io_uring_queue_init_params succeeded, so exit is due
};

UringRing::UringRing() : state_(std::make_unique<State>()) {}

UringRing::~UringRing() {
    if (state_ && state_->initialised) {
        io_uring_queue_exit(&state_->ring);
    }
}

std::unique_ptr<UringRing> UringRing::create(const RingOptions& options, const UringRing* share_kernel_thread_with,
                                             std::string* error, int* error_code) {
    std::unique_ptr<UringRing> self(new UringRing());
    struct io_uring_params params{};
    params.flags = IORING_SETUP_CLAMP | IORING_SETUP_CQSIZE;
    params.cq_entries = options.depth * 4;
    if (options.sqpoll) {
        params.flags |= IORING_SETUP_SQPOLL;
        params.sq_thread_idle = options.sqpoll_idle_ms;
        if (options.sqpoll_cpu >= 0) {
            params.flags |= IORING_SETUP_SQ_AFF;
            params.sq_thread_cpu = static_cast<uint32_t>(options.sqpoll_cpu);
        }
        if (share_kernel_thread_with != nullptr && share_kernel_thread_with->sqpoll_) {
            params.flags |= IORING_SETUP_ATTACH_WQ;
            params.wq_fd = static_cast<uint32_t>(share_kernel_thread_with->state_->ring.ring_fd);
        }
    }
    int rc = io_uring_queue_init_params(options.depth, &self->state_->ring, &params);
    if (rc < 0) {
        *error_code = -rc;
        *error = std::string("io_uring_queue_init_params: ") + strerror(-rc);
        return nullptr;
    }
    self->state_->initialised = true;
    self->sqpoll_ = options.sqpoll;

    if (!(params.features & IORING_FEAT_EXT_ARG) || !(params.features & IORING_FEAT_FAST_POLL)) {
        *error_code = ENOSYS;
        *error = "this kernel's io_uring lacks IORING_FEAT_EXT_ARG or IORING_FEAT_FAST_POLL (Linux 5.11 or later is needed)";
        return nullptr;
    }
    struct io_uring_probe* probe = io_uring_get_probe_ring(&self->state_->ring);
    if (probe == nullptr) {
        *error_code = ENOSYS;
        *error = "this kernel cannot say which io_uring operations it has (Linux 5.11 or later is needed)";
        return nullptr;
    }
    for (int op : REQUIRED_OPS) {
        if (!io_uring_opcode_supported(probe, op)) {
            io_uring_free_probe(probe);
            *error_code = ENOSYS;
            *error = "this kernel's io_uring lacks operation " + std::to_string(op);
            return nullptr;
        }
    }
    io_uring_free_probe(probe);

    // Optional: a kernel that refuses them leaves the ring working on plain descriptors and buffers.
    if (options.file_slots > 0) {
        rc = io_uring_register_files_sparse(&self->state_->ring, options.file_slots);
        if (rc < 0) {
            std::vector<int> empty(options.file_slots, -1);
            rc = io_uring_register_files(&self->state_->ring, empty.data(), options.file_slots);
        }
        if (rc == 0) {
            self->file_slots_ = options.file_slots;
        }
    }
    if (options.buffer_slots > 0
        && io_uring_register_buffers_sparse(&self->state_->ring, options.buffer_slots) == 0) {
        self->buffer_slots_ = options.buffer_slots;
    }
    return self;
}

unsigned UringRing::sq_entries() const noexcept { return state_->ring.sq.ring_entries; }
unsigned UringRing::cq_entries() const noexcept { return state_->ring.cq.ring_entries; }

int UringRing::set_file(unsigned slot, int fd) {
    const int rc = io_uring_register_files_update(&state_->ring, slot, &fd, 1);
    return rc < 0 ? rc : 0;
}

int UringRing::set_buffer(unsigned slot, void* base, size_t len) {
    struct iovec iov{base, base != nullptr ? len : 0};
    __u64 tag = 0;
    const int rc = io_uring_register_buffers_update_tag(&state_->ring, slot, &iov, &tag, 1);
    return rc < 0 ? rc : 0;
}

namespace {

inline struct io_uring_sqe* next_sqe(struct io_uring& ring) {
    return io_uring_get_sqe(&ring);
}

inline void finish(struct io_uring_sqe* sqe, RingFile file, uint64_t user_data) {
    if (file.slot >= 0) {
        sqe->flags |= IOSQE_FIXED_FILE;
    }
    io_uring_sqe_set_data64(sqe, user_data);
}

inline int target(RingFile file) { return file.slot >= 0 ? file.slot : file.fd; }

}  // namespace

bool UringRing::accept(RingFile file, sockaddr* address, socklen_t* length, uint64_t user_data) {
    struct io_uring_sqe* sqe = next_sqe(state_->ring);
    if (sqe == nullptr) return false;
    io_uring_prep_accept(sqe, target(file), address, length, SOCK_CLOEXEC);
    finish(sqe, file, user_data);
    return true;
}

bool UringRing::recv(RingFile file, void* buffer, size_t length, int flags, uint64_t user_data) {
    struct io_uring_sqe* sqe = next_sqe(state_->ring);
    if (sqe == nullptr) return false;
    io_uring_prep_recv(sqe, target(file), buffer, length, flags);
    finish(sqe, file, user_data);
    return true;
}

bool UringRing::read_fixed(RingFile file, void* buffer, size_t length, unsigned buffer_slot, uint64_t user_data) {
    struct io_uring_sqe* sqe = next_sqe(state_->ring);
    if (sqe == nullptr) return false;
    io_uring_prep_read_fixed(sqe, target(file), buffer, static_cast<unsigned>(length), 0,
                             static_cast<int>(buffer_slot));
    finish(sqe, file, user_data);
    return true;
}

bool UringRing::recvmsg(RingFile file, msghdr* message, int flags, uint64_t user_data) {
    struct io_uring_sqe* sqe = next_sqe(state_->ring);
    if (sqe == nullptr) return false;
    io_uring_prep_recvmsg(sqe, target(file), message, static_cast<unsigned>(flags));
    finish(sqe, file, user_data);
    return true;
}

bool UringRing::send(RingFile file, const void* buffer, size_t length, int flags, uint64_t user_data) {
    struct io_uring_sqe* sqe = next_sqe(state_->ring);
    if (sqe == nullptr) return false;
    io_uring_prep_send(sqe, target(file), buffer, length, flags);
    finish(sqe, file, user_data);
    return true;
}

bool UringRing::sendmsg(RingFile file, const msghdr* message, int flags, uint64_t user_data) {
    struct io_uring_sqe* sqe = next_sqe(state_->ring);
    if (sqe == nullptr) return false;
    io_uring_prep_sendmsg(sqe, target(file), message, static_cast<unsigned>(flags));
    finish(sqe, file, user_data);
    return true;
}

bool UringRing::read_eventfd(int fd, uint64_t* value, uint64_t user_data) {
    struct io_uring_sqe* sqe = next_sqe(state_->ring);
    if (sqe == nullptr) return false;
    io_uring_prep_read(sqe, fd, value, sizeof(*value), 0);
    io_uring_sqe_set_data64(sqe, user_data);
    return true;
}

bool UringRing::cancel(uint64_t target_data, uint64_t user_data) {
    struct io_uring_sqe* sqe = next_sqe(state_->ring);
    if (sqe == nullptr) return false;
    io_uring_prep_cancel64(sqe, target_data, 0);
    io_uring_sqe_set_data64(sqe, user_data);
    return true;
}

int UringRing::submit() {
    return io_uring_submit(&state_->ring);
}

int UringRing::reap(std::vector<RingCompletion>& out) {
    unsigned head;
    unsigned seen = 0;
    struct io_uring_cqe* cqe;
    io_uring_for_each_cqe(&state_->ring, head, cqe) {
        ++seen;
        if (cqe->user_data == LIBURING_UDATA_TIMEOUT) {
            continue;
        }
        out.push_back(RingCompletion{cqe->user_data, cqe->res, cqe->flags});
    }
    if (seen > 0) {
        io_uring_cq_advance(&state_->ring, seen);  // once for the whole batch
    }
    return static_cast<int>(seen);
}

int UringRing::wait(std::vector<RingCompletion>& out, unsigned wanted, int64_t timeout_us, int64_t spin_us) {
    const size_t before = out.size();
    auto have = [&] { return static_cast<unsigned>(out.size() - before); };
    if (spin_us > 0) {
        const int rc = io_uring_submit(&state_->ring);
        if (rc < 0 && rc != -EAGAIN && rc != -EBUSY && rc != -EINTR) {
            return rc;
        }
        const int64_t start = now_us();
        const int64_t budget = spin_us < timeout_us ? spin_us : timeout_us;
        for (;;) {
            reap(out);
            if (have() >= wanted) {
                return static_cast<int>(have());
            }
            if (now_us() - start >= budget) {
                break;
            }
            cpu_relax();
        }
        timeout_us -= now_us() - start;
        if (timeout_us <= 0) {
            return static_cast<int>(have());
        }
    }
    reap(out);
    if (have() < wanted) {
        struct __kernel_timespec ts{};
        ts.tv_sec = timeout_us / 1'000'000;
        ts.tv_nsec = (timeout_us % 1'000'000) * 1000;
        struct io_uring_cqe* cqe = nullptr;
        const int rc = io_uring_submit_and_wait_timeout(&state_->ring, &cqe, wanted - have(), &ts, nullptr);
        if (rc < 0 && rc != -ETIME && rc != -EINTR && rc != -EAGAIN && rc != -EBUSY) {
            return rc;
        }
        reap(out);
    } else {
        // Enough is there already; still hand over what was prepared.
        const int rc = io_uring_submit(&state_->ring);
        if (rc < 0 && rc != -EAGAIN && rc != -EBUSY && rc != -EINTR) {
            return rc;
        }
    }
    return static_cast<int>(have());
}

}  // namespace libe3
