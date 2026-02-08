#pragma once

#include <cstddef>
#include <optional>
#include <rigtorp/SPSCQueue.h>

namespace cme::sim {

// ---------------------------------------------------------------------------
// Thin wrapper around rigtorp::SPSCQueue for single-producer / single-consumer
// lock-free communication between threads.
// ---------------------------------------------------------------------------
template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(std::size_t capacity)
        : queue_(capacity) {}

    // Non-copyable, non-movable
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    // Try to push an element. Returns false if the queue is full.
    bool tryPush(const T& value) {
        return queue_.try_push(value);
    }

    bool tryPush(T&& value) {
        return queue_.try_push(std::move(value));
    }

    // Try to emplace an element in-place. Returns false if the queue is full.
    template <typename... Args>
    bool tryEmplace(Args&&... args) {
        return queue_.try_emplace(std::forward<Args>(args)...);
    }

    // Try to pop an element. Returns std::nullopt if the queue is empty.
    std::optional<T> tryPop() {
        T* front = queue_.front();
        if (!front) {
            return std::nullopt;
        }
        T value = std::move(*front);
        queue_.pop();
        return value;
    }

    // Peek at the front element without removing. Returns nullptr if empty.
    T* front() {
        return queue_.front();
    }

    // Pop the front element (caller must have checked front() != nullptr).
    void pop() {
        queue_.pop();
    }

    // Approximate size (not guaranteed accurate under contention).
    std::size_t size() const {
        return queue_.size();
    }

    bool empty() const {
        return queue_.empty();
    }

private:
    rigtorp::SPSCQueue<T> queue_;
};

} // namespace cme::sim
