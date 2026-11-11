/**
 * @file uring_ring.hpp
 * @brief One io_uring instance, without liburing in the interface
 *
 * The connector speaks to the ring through this class only, so <liburing.h> is included by
 * uring_ring.cpp alone. Completions come back as plain RingCompletion values.
 *
 * Not thread-safe: the caller lets one thread at a time prepare, submit and reap (the
 * connector holds a mutex per ring). Registering files and buffers may come from any thread.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_URING_RING_HPP
#define LIBE3_URING_RING_HPP

#include <sys/socket.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace libe3 {

/** @brief A completion, as the ring reported it. */
struct RingCompletion {
    uint64_t user_data;  ///< What the operation was prepared with
    int32_t res;         ///< Its result: bytes or a descriptor, or -errno
    uint32_t flags;      ///< IORING_CQE_F_* flags
};

/** @brief How a ring is set up. */
struct RingOptions {
    unsigned depth{64};          ///< Submission queue entries; the completion queue has four times as many
    bool sqpoll{false};          ///< A kernel thread takes the submissions, so submitting needs no system call
    int sqpoll_cpu{-1};          ///< The CPU of that thread, or -1 to let the scheduler choose
    unsigned sqpoll_idle_ms{1000};  ///< How long that thread polls before it sleeps
    unsigned file_slots{0};      ///< Registered-file slots to reserve; 0 for none
    unsigned buffer_slots{0};    ///< Registered-buffer slots to reserve; 0 for none
};

/** @brief A socket an operation acts on: by descriptor, or by registered-file slot. */
struct RingFile {
    int fd;     ///< The descriptor
    int slot;   ///< Its registered-file slot, or -1 to use the descriptor
};

/** @brief One io_uring instance. */
class UringRing {
public:
    /**
     * @brief Set up a ring.
     * @param options How
     * @param share_kernel_thread_with A ring whose kernel submission thread this one uses (with sqpoll), or null
     * @param error Set to the reason when the ring cannot be set up
     * @param error_code Set to the errno of that reason
     * @return The ring, or null
     */
    static std::unique_ptr<UringRing> create(const RingOptions& options, const UringRing* share_kernel_thread_with,
                                             std::string* error, int* error_code);

    /** @brief Tear the ring down; the kernel cancels what is still in flight. */
    ~UringRing();

    UringRing(const UringRing&) = delete;
    UringRing& operator=(const UringRing&) = delete;

    /**
     * @brief Submission queue entries the kernel granted.
     * @return The number of entries
     */
    unsigned sq_entries() const noexcept;
    /**
     * @brief Completion queue entries the kernel granted.
     * @return The number of entries
     */
    unsigned cq_entries() const noexcept;
    /**
     * @brief Whether a kernel thread takes the submissions.
     * @return true with IORING_SETUP_SQPOLL
     */
    bool sqpoll() const noexcept { return sqpoll_; }
    /**
     * @brief Registered-file slots reserved.
     * @return Their number, 0 when none were asked for or the kernel refused them
     */
    unsigned file_slots() const noexcept { return file_slots_; }
    /**
     * @brief Registered-buffer slots reserved.
     * @return Their number, 0 when none were asked for or the kernel refused them
     */
    unsigned buffer_slots() const noexcept { return buffer_slots_; }

    /**
     * @brief Put a descriptor in a registered-file slot, or empty the slot.
     * @param slot The slot
     * @param fd The descriptor, or -1 to empty it
     * @return 0 or -errno
     */
    int set_file(unsigned slot, int fd);
    /**
     * @brief Register a buffer in a slot, or empty the slot.
     * @param slot The slot
     * @param base The buffer, or null to empty it
     * @param len Its length
     * @return 0 or -errno
     */
    int set_buffer(unsigned slot, void* base, size_t len);

    /**
     * @brief Prepare an accept.
     * @param file The listening socket
     * @param address Where the peer's address goes; must stay valid until the completion
     * @param length Its room, then its length; must stay valid until the completion
     * @param user_data What the completion carries
     * @return false when the submission queue is full
     */
    bool accept(RingFile file, sockaddr* address, socklen_t* length, uint64_t user_data);
    /**
     * @brief Prepare a receive.
     * @param file The socket
     * @param buffer Where the bytes go; must stay valid until the completion
     * @param length Its room
     * @param flags recv flags
     * @param user_data What the completion carries
     * @return false when the submission queue is full
     */
    bool recv(RingFile file, void* buffer, size_t length, int flags, uint64_t user_data);
    /**
     * @brief Prepare a receive into a registered buffer.
     * @param file The socket
     * @param buffer Where the bytes go, inside the registered buffer
     * @param length Its room
     * @param buffer_slot The slot of the registered buffer
     * @param user_data What the completion carries
     * @return false when the submission queue is full
     */
    bool read_fixed(RingFile file, void* buffer, size_t length, unsigned buffer_slot, uint64_t user_data);
    /**
     * @brief Prepare a recvmsg.
     * @param file The socket
     * @param message The message header; the kernel writes its flags and control data back, so it
     *        must stay valid until the completion
     * @param flags recvmsg flags
     * @param user_data What the completion carries
     * @return false when the submission queue is full
     */
    bool recvmsg(RingFile file, msghdr* message, int flags, uint64_t user_data);
    /**
     * @brief Prepare a send.
     * @param file The socket
     * @param buffer The bytes; must stay valid until the completion
     * @param length How many
     * @param flags send flags
     * @param user_data What the completion carries
     * @return false when the submission queue is full
     */
    bool send(RingFile file, const void* buffer, size_t length, int flags, uint64_t user_data);
    /**
     * @brief Prepare a sendmsg.
     * @param file The socket
     * @param message The message; it and what it points to must stay valid until the completion
     * @param flags sendmsg flags
     * @param user_data What the completion carries
     * @return false when the submission queue is full
     */
    bool sendmsg(RingFile file, const msghdr* message, int flags, uint64_t user_data);
    /**
     * @brief Prepare a read of an eventfd's eight bytes.
     * @param fd The eventfd
     * @param value Where the count goes; must stay valid until the completion
     * @param user_data What the completion carries
     * @return false when the submission queue is full
     */
    bool read_eventfd(int fd, uint64_t* value, uint64_t user_data);
    /**
     * @brief Prepare the cancellation of an operation.
     * @param target The user data the operation was prepared with
     * @param user_data What the cancellation's own completion carries
     * @return false when the submission queue is full
     */
    bool cancel(uint64_t target, uint64_t user_data);

    /**
     * @brief Hand what was prepared to the kernel.
     * @return How many, or -errno
     */
    int submit();

    /**
     * @brief Submit what was prepared, then gather completions.
     *
     * Returns once at least @p wanted completions are there or @p timeout_us has passed. For
     * the first @p spin_us of that it polls the completion queue instead of sleeping.
     *
     * @param out Completions are appended here
     * @param wanted How many to wait for
     * @param timeout_us How long to wait at most, in microseconds
     * @param spin_us How long to poll before sleeping, in microseconds
     * @return How many were appended, or -errno for an error of the ring itself
     */
    int wait(std::vector<RingCompletion>& out, unsigned wanted, int64_t timeout_us, int64_t spin_us);

    /**
     * @brief Append the completions that are there, without waiting.
     * @param out Where they are appended
     * @return How many the queue held
     */
    int reap(std::vector<RingCompletion>& out);

private:
    UringRing();
    struct State;
    std::unique_ptr<State> state_;
    bool sqpoll_{false};
    unsigned file_slots_{0};
    unsigned buffer_slots_{0};
};

}  // namespace libe3

#endif  // LIBE3_URING_RING_HPP
