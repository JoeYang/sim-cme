#pragma once
#include "order.h"

namespace cme::sim {

class PriceLevel {
public:
    Price price;
    Quantity total_quantity = 0;
    int order_count = 0;

    void addOrder(Order* order) {
        order->prev_in_level = tail_;
        order->next_in_level = nullptr;
        if (tail_) {
            tail_->next_in_level = order;
        } else {
            head_ = order;
        }
        tail_ = order;
        total_quantity += order->remainingQty();
        ++order_count;
    }

    void removeOrder(Order* order) {
        if (order->prev_in_level) {
            order->prev_in_level->next_in_level = order->next_in_level;
        } else {
            head_ = order->next_in_level;
        }
        if (order->next_in_level) {
            order->next_in_level->prev_in_level = order->prev_in_level;
        } else {
            tail_ = order->prev_in_level;
        }
        total_quantity -= order->remainingQty();
        --order_count;
        order->prev_in_level = nullptr;
        order->next_in_level = nullptr;
    }

    Order* front() const { return head_; }
    bool empty() const { return head_ == nullptr; }

    // Iterator for walking orders in FIFO order
    class Iterator {
    public:
        using value_type = Order*;
        using difference_type = std::ptrdiff_t;
        using pointer = Order**;
        using reference = Order*&;
        using iterator_category = std::forward_iterator_tag;

        explicit Iterator(Order* node) : current_(node) {}
        Order* operator*() const { return current_; }
        Iterator& operator++() { current_ = current_->next_in_level; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
        bool operator==(const Iterator& o) const { return current_ == o.current_; }
        bool operator!=(const Iterator& o) const { return current_ != o.current_; }
    private:
        Order* current_;
    };

    Iterator begin() const { return Iterator(head_); }
    Iterator end() const { return Iterator(nullptr); }

private:
    Order* head_ = nullptr;
    Order* tail_ = nullptr;
};

} // namespace cme::sim
