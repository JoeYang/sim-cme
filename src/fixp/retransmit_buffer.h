#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace cme::sim::fixp {

// Circular buffer storing recent outbound messages for retransmission.
// Each entry is keyed by its outbound sequence number.
class RetransmitBuffer {
public:
    explicit RetransmitBuffer(size_t capacity = 10000)
        : capacity_(capacity)
        , entries_(capacity) {}

    // Store a copy of an outbound message (SBE payload, no SOFH).
    void store(uint32_t seq_num, const char* data, size_t len) {
        size_t idx = seq_num % capacity_;
        entries_[idx].seq_num = seq_num;
        entries_[idx].data.assign(data, data + len);
    }

    // Retrieve a previously-stored message by sequence number.
    // Returns nullptr and sets out_len=0 if not found or overwritten.
    const char* retrieve(uint32_t seq_num, size_t& out_len) const {
        size_t idx = seq_num % capacity_;
        if (entries_[idx].seq_num == seq_num && !entries_[idx].data.empty()) {
            out_len = entries_[idx].data.size();
            return entries_[idx].data.data();
        }
        out_len = 0;
        return nullptr;
    }

    size_t capacity() const { return capacity_; }

private:
    struct Entry {
        uint32_t seq_num = 0;
        std::vector<char> data;
    };

    size_t capacity_;
    std::vector<Entry> entries_;
};

} // namespace cme::sim::fixp
