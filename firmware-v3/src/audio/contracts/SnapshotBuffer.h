#pragma once
#include <atomic>
#include <new>
#include <stddef.h>
#include <stdint.h>

#include "../../config/Trace.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos::audio {

/**
 * @brief Lock-free double buffer (publish on one core, read on another).
 *
 * - Writer calls Publish(value). No dynamic allocation.
 * - Reader calls ReadLatest(out) and receives a BY-VALUE copy.
 * - Sequence increments on each publish; reader can detect staleness.
 */
template <typename T>
class SnapshotBuffer final {
public:
    SnapshotBuffer() = default;

    SnapshotBuffer(const SnapshotBuffer&) = delete;
    SnapshotBuffer& operator=(const SnapshotBuffer&) = delete;
    SnapshotBuffer(SnapshotBuffer&&) = delete;
    SnapshotBuffer& operator=(SnapshotBuffer&&) = delete;

    /**
     * @brief Publish a new snapshot (writer thread).
     */
    void Publish(const T& v) {
        // Write into the inactive buffer first.
        const uint32_t cur = m_active.load(std::memory_order_relaxed);
        
        // DEFENSIVE CHECK: Validate cur is 0 or 1 (defensive check)
        // SnapshotBuffer uses double-buffering with m_buf[2] array. The XOR
        // operation (cur ^ 1U) should always produce 0 or 1, but if cur is
        // corrupted, this ensures we use a safe index.
        uint32_t safe_cur = (cur > 1) ? 0 : cur;
        const uint32_t nxt = safe_cur ^ 1U;
        
        // DEFENSIVE CHECK: Ensure nxt is 0 or 1 (should never be >1 after XOR, but protects against corruption)
        if (nxt > 1) {
            // Should never happen, but defensive - write to buffer 0 and reset state
            m_buf[0] = v;
            m_active.store(0, std::memory_order_release);
            m_seq.fetch_add(1U, std::memory_order_release);
            return;
        }

        m_buf[nxt] = v;

        // Make sure payload write lands before we flip active + seq.
        std::atomic_thread_fence(std::memory_order_release);
        m_active.store(nxt, std::memory_order_release);
        m_seq.fetch_add(1U, std::memory_order_release);
    }

    /**
     * @brief Read latest snapshot by value (reader thread). Returns sequence id.
     *
     * If writer publishes during the copy, we retry once.
     */
    uint32_t ReadLatest(T& out) const {
        uint32_t s0 = m_seq.load(std::memory_order_acquire);
        uint32_t idx = m_active.load(std::memory_order_acquire);
        
        // DEFENSIVE CHECK: Validate idx is 0 or 1 (defensive check)
        // SnapshotBuffer uses double-buffering with m_buf[2] array. If m_active
        // is corrupted (e.g., by memory corruption), accessing m_buf[idx] would
        // cause out-of-bounds access and crash. This validation ensures we always
        // use a valid index.
        if (idx > 1) {
            idx = 0;  // Reset to safe default if corrupted
        }

#if FEATURE_TRACE_AUDIO_HANDOFF
        {
            TRACE_SCOPE("bus_copy_memcpy");
            out = m_buf[idx];
        }
#else
        out = m_buf[idx];
#endif

        std::atomic_thread_fence(std::memory_order_acquire);
        uint32_t s1 = m_seq.load(std::memory_order_acquire);

        const bool seqChanged = (s1 != s0);
#if FEATURE_TRACE_AUDIO_HANDOFF
        TRACE_COUNTER("bus_retry_check", seqChanged ? 1 : 0);
#endif

        if (seqChanged) {
            // One retry for consistency.
#if FEATURE_TRACE_AUDIO_HANDOFF
            TRACE_INSTANT("bus_retry_check");
#endif
            m_retryCount.fetch_add(1U, std::memory_order_relaxed);
            idx = m_active.load(std::memory_order_acquire);
            // Validate idx again after reload
            if (idx > 1) {
                idx = 0;
            }
#if FEATURE_TRACE_AUDIO_HANDOFF
            {
                TRACE_SCOPE("bus_copy_retry");
                out = m_buf[idx];
            }
#else
            out = m_buf[idx];
#endif
            s1 = m_seq.load(std::memory_order_acquire);
        }
        return s1;
    }

    /**
     * @brief Current sequence counter (monotonic, wraps at 2^32).
     */
    uint32_t Sequence() const { return m_seq.load(std::memory_order_acquire); }

    /**
     * @brief Cumulative read retry count for cross-core copy diagnostics.
     */
    uint32_t RetryCount() const { return m_retryCount.load(std::memory_order_relaxed); }

    /**
     * @brief Address of the backing payload array for placement diagnostics.
     */
    const void* StorageAddressForDiagnostics() const { return static_cast<const void*>(&m_buf[0]); }

    /**
     * @brief Bytes occupied by the double-buffered payload storage.
     */
    size_t PayloadBytesForDiagnostics() const { return sizeof(m_buf); }

    /**
     * @brief Bytes occupied by the full SnapshotBuffer object.
     */
    size_t ObjectBytesForDiagnostics() const { return sizeof(*this); }

private:
    // Align to T to satisfy platforms with stricter alignment than 4 bytes.
    alignas(T) T m_buf[2]{};
    mutable std::atomic<uint32_t> m_active{0};
    mutable std::atomic<uint32_t> m_seq{0};
    mutable std::atomic<uint32_t> m_retryCount{0};
};

/**
 * @brief Owns a SnapshotBuffer in internal memory on ESP targets.
 *
 * The owner allocates once during construction. Hot-path publish/read calls still
 * operate on SnapshotBuffer directly and perform no allocation.
 */
template <typename T>
class InternalSnapshotBufferOwner final {
public:
    using Buffer = SnapshotBuffer<T>;

    InternalSnapshotBufferOwner() {
#ifdef NATIVE_BUILD
        m_buffer = new (m_storage) Buffer();
#else
        m_storage = heap_caps_malloc(sizeof(Buffer), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (m_storage != nullptr) {
            m_buffer = new (m_storage) Buffer();
        }
#endif
    }

    ~InternalSnapshotBufferOwner() {
        if (m_buffer != nullptr) {
            m_buffer->~Buffer();
            m_buffer = nullptr;
        }
#ifndef NATIVE_BUILD
        if (m_storage != nullptr) {
            heap_caps_free(m_storage);
            m_storage = nullptr;
        }
#endif
    }

    InternalSnapshotBufferOwner(const InternalSnapshotBufferOwner&) = delete;
    InternalSnapshotBufferOwner& operator=(const InternalSnapshotBufferOwner&) = delete;
    InternalSnapshotBufferOwner(InternalSnapshotBufferOwner&&) = delete;
    InternalSnapshotBufferOwner& operator=(InternalSnapshotBufferOwner&&) = delete;

    bool IsReady() const { return m_buffer != nullptr; }
    Buffer* get() { return m_buffer; }
    const Buffer* get() const { return m_buffer; }
    Buffer& operator*() { return *m_buffer; }
    const Buffer& operator*() const { return *m_buffer; }
    Buffer* operator->() { return m_buffer; }
    const Buffer* operator->() const { return m_buffer; }

private:
    Buffer* m_buffer = nullptr;
#ifdef NATIVE_BUILD
    alignas(Buffer) unsigned char m_storage[sizeof(Buffer)]{};
#else
    void* m_storage = nullptr;
#endif
};

} // namespace lightwaveos::audio
