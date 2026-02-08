#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace cme::sim {

// ---------------------------------------------------------------------------
// Pre-allocated pool of fixed-size buffers for zero-allocation message
// building.  Thread-safe acquire / release using a lock-free free-list.
//
// Typical usage:
//   BufferPool pool(256, 4096);   // 256 buffers of 4 KB each
//   auto* buf = pool.acquire();   // nullptr if exhausted
//   // ... write SBE message into buf->data ...
//   pool.release(buf);
// ---------------------------------------------------------------------------

struct PoolBuffer {
    uint8_t* data;
    std::size_t capacity;
    std::size_t length; // bytes actually written

    void reset() { length = 0; }
};

class BufferPool {
public:
    // Create a pool of `count` buffers, each of `bufferSize` bytes.
    BufferPool(std::size_t count, std::size_t bufferSize)
        : bufferSize_(bufferSize) {
        // Single contiguous allocation for all buffer memory.
        storage_.resize(count * bufferSize);

        // Pre-build buffer descriptors and push them onto the free list.
        buffers_.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            buffers_[i].data     = storage_.data() + (i * bufferSize);
            buffers_[i].capacity = bufferSize;
            buffers_[i].length   = 0;

            auto* node   = new FreeNode;
            node->buffer = &buffers_[i];
            node->next   = freeList_.load(std::memory_order_relaxed);
            freeList_.store(node, std::memory_order_relaxed);
        }
    }

    ~BufferPool() {
        // Drain free list and delete nodes.
        FreeNode* node = freeList_.load(std::memory_order_relaxed);
        while (node) {
            FreeNode* next = node->next;
            delete node;
            node = next;
        }
    }

    // Non-copyable, non-movable
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    // Acquire a buffer from the pool. Returns nullptr if the pool is exhausted.
    PoolBuffer* acquire() {
        FreeNode* node = popFreeList();
        if (!node) {
            return nullptr;
        }
        PoolBuffer* buf = node->buffer;
        buf->reset();
        delete node;
        return buf;
    }

    // Release a buffer back to the pool.
    void release(PoolBuffer* buf) {
        assert(buf != nullptr);
        buf->reset();
        auto* node   = new FreeNode;
        node->buffer = buf;
        pushFreeList(node);
    }

    std::size_t bufferSize() const { return bufferSize_; }

private:
    struct FreeNode {
        PoolBuffer* buffer = nullptr;
        FreeNode*   next   = nullptr;
    };

    FreeNode* popFreeList() {
        FreeNode* head = freeList_.load(std::memory_order_acquire);
        while (head) {
            if (freeList_.compare_exchange_weak(
                    head, head->next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return head;
            }
        }
        return nullptr; // pool exhausted
    }

    void pushFreeList(FreeNode* node) {
        node->next = freeList_.load(std::memory_order_relaxed);
        while (!freeList_.compare_exchange_weak(
                   node->next, node,
                   std::memory_order_release,
                   std::memory_order_relaxed)) {
            // CAS retry; node->next is updated by the CAS failure
        }
    }

    std::size_t              bufferSize_;
    std::vector<uint8_t>     storage_;  // contiguous backing memory
    std::vector<PoolBuffer>  buffers_;  // buffer descriptors
    std::atomic<FreeNode*>   freeList_{nullptr};
};

} // namespace cme::sim
