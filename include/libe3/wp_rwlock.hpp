/**
 * @file wp_rwlock.hpp
 * @brief Writer-preferring reader/writer lock built on pthread_rwlock_t.
 *
 * std::shared_mutex on glibc maps to pthread_rwlock_t with the default
 * "prefer reader" attribute. Under sustained reader pressure (e.g. an
 * SM worker calling get_subscribed_dapps / get_dapp_channel on every
 * emission at hundreds of Hz) that lets writers — most importantly
 * register_dapp called from setup_loop — wait for an unbounded queue
 * of readers to drain. The observable symptom is that a new dApp's
 * setup REQ times out at 1 s when the gNB is otherwise serving an
 * active subscriber at high cadence.
 *
 * This class wraps pthread_rwlock_t with
 * PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP, which keeps reader
 * concurrency for the common case but blocks new readers as soon as a
 * writer is queued. New readers wait, the queued writer runs as soon
 * as the in-flight readers release, and writer latency becomes
 * bounded by the longest individual reader's critical section.
 *
 * Interface is BasicLockable + SharedLockable so std::unique_lock and
 * std::shared_lock work transparently — callers swap the mutex type
 * and existing RAII guards keep compiling.
 *
 * Linux glibc only. On macOS / musl the _NP attribute isn't available;
 * fall back to default attrs (POSIX-undefined preference) — still
 * compiles, no behavioural promise but no degradation either.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIBE3_WP_RWLOCK_HPP
#define LIBE3_WP_RWLOCK_HPP

#include <pthread.h>

namespace libe3 {

class WriterPreferringRwLock {
public:
    WriterPreferringRwLock() noexcept {
        pthread_rwlockattr_t attr;
        pthread_rwlockattr_init(&attr);
#if defined(__GLIBC__) && defined(PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP)
        pthread_rwlockattr_setkind_np(
            &attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif
        pthread_rwlock_init(&lock_, &attr);
        pthread_rwlockattr_destroy(&attr);
    }

    ~WriterPreferringRwLock() noexcept {
        pthread_rwlock_destroy(&lock_);
    }

    WriterPreferringRwLock(const WriterPreferringRwLock&) = delete;
    WriterPreferringRwLock& operator=(const WriterPreferringRwLock&) = delete;
    WriterPreferringRwLock(WriterPreferringRwLock&&) = delete;
    WriterPreferringRwLock& operator=(WriterPreferringRwLock&&) = delete;

    /* BasicLockable (writer side) — for std::unique_lock. */
    void lock() noexcept   { pthread_rwlock_wrlock(&lock_); }
    void unlock() noexcept { pthread_rwlock_unlock(&lock_); }
    bool try_lock() noexcept {
        return pthread_rwlock_trywrlock(&lock_) == 0;
    }

    /* SharedLockable (reader side) — for std::shared_lock. */
    void lock_shared() noexcept   { pthread_rwlock_rdlock(&lock_); }
    void unlock_shared() noexcept { pthread_rwlock_unlock(&lock_); }
    bool try_lock_shared() noexcept {
        return pthread_rwlock_tryrdlock(&lock_) == 0;
    }

private:
    pthread_rwlock_t lock_;
};

} // namespace libe3

#endif // LIBE3_WP_RWLOCK_HPP
