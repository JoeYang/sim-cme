#pragma once

#include <atomic>
#include <cstddef>
#include <optional>
#include <utility>

namespace cme::sim {

// ---------------------------------------------------------------------------
// Lock-free multi-producer / single-consumer queue.
//
// Uses a Michael-Scott-style intrusive linked list with atomic push (CAS on
// head) and single-consumer pop from the tail. Nodes are heap-allocated on
// push and freed on pop -- suitable for moderate-throughput control paths
// (e.g., gateway -> engine command submission).
//
// For the hot path (market data fan-out, order matching) prefer SPSCQueue.
// ---------------------------------------------------------------------------
template <typename T>
class MPSCQueue {
public:
    MPSCQueue() {
        // Sentinel node so that head_ is never null.
        Node* sentinel = new Node{};
        head_.store(sentinel, std::memory_order_relaxed);
        tail_ = sentinel;
    }

    ~MPSCQueue() {
        // Drain remaining nodes.
        while (tryPop().has_value()) {}
        // Free the sentinel.
        delete tail_;
    }

    // Non-copyable, non-movable
    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;

    // Push (thread-safe, multiple producers).
    void push(const T& value) {
        auto* node = new Node{value};
        pushNode(node);
    }

    void push(T&& value) {
        auto* node = new Node{std::move(value)};
        pushNode(node);
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        auto* node = new Node{T{std::forward<Args>(args)...}};
        pushNode(node);
    }

    // Pop (single consumer only). Returns std::nullopt if empty.
    std::optional<T> tryPop() {
        Node* sentinel = tail_;
        Node* next = sentinel->next.load(std::memory_order_acquire);
        if (!next) {
            return std::nullopt;
        }
        // Advance tail past sentinel; sentinel is freed.
        T value = std::move(next->data);
        tail_ = next;
        delete sentinel;
        return value;
    }

    // Check if empty (consumer side; may race with concurrent pushes).
    bool empty() const {
        return tail_->next.load(std::memory_order_acquire) == nullptr;
    }

private:
    struct Node {
        T data{};
        std::atomic<Node*> next{nullptr};

        Node() = default;
        explicit Node(const T& v) : data(v) {}
        explicit Node(T&& v) : data(std::move(v)) {}
    };

    void pushNode(Node* node) {
        // Prepend to the stack (head_) -- but we actually want FIFO order, so
        // we use the "reverse push" technique: each producer writes its node's
        // next to the old head, then CAS head to the new node. The consumer
        // walks next pointers from tail.
        //
        // Correct FIFO MPSC: producer sets node->next = nullptr, then stores
        // into prev->next after xchg on head_.
        node->next.store(nullptr, std::memory_order_relaxed);
        Node* prev = head_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    // head_ is the last node pushed (newest). Producers CAS on it.
    alignas(64) std::atomic<Node*> head_{nullptr};
    // tail_ is the sentinel before the oldest unconsumed node. Consumer only.
    alignas(64) Node* tail_{nullptr};
};

} // namespace cme::sim
