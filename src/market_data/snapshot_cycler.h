#pragma once

#include "../common/types.h"
#include "feed_sender.h"
#include "snapshot_builder.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>
#include <utility>
#include <vector>

namespace cme::sim::market_data {

/// Dedicated component that continuously cycles through instruments, building
/// and sending SnapshotFullRefresh52 messages on both Feed A and Feed B.
/// Designed to run on its own background thread.
class SnapshotCycler {
public:
    /// Callback to retrieve the current book state for a given security.
    using BookSnapshotProvider = std::function<void(
        SecurityId sec_id,
        std::vector<std::pair<Price, Quantity>>& bids,
        std::vector<std::pair<Price, Quantity>>& asks,
        std::vector<int>& bid_counts,
        std::vector<int>& ask_counts)>;

    SnapshotCycler(const std::vector<SecurityId>& instruments,
                   BookSnapshotProvider provider,
                   FeedSender& feed_a,
                   FeedSender& feed_b);

    /// Run one snapshot cycle (all instruments). Called externally or from
    /// the background thread.
    void runCycle(uint32_t last_incremental_seq_num);

    /// Start continuous snapshot cycling on a background thread.
    void start();

    /// Stop the background thread.
    void stop();

    /// Whether the cycler is currently running.
    bool isRunning() const { return running_.load(std::memory_order_relaxed); }

    /// Set the last incremental sequence number for snapshot recovery sync.
    void setLastIncrementalSeqNum(uint32_t seq) {
        last_incr_seq_.store(seq, std::memory_order_relaxed);
    }

private:
    std::vector<SecurityId> instruments_;
    BookSnapshotProvider provider_;
    FeedSender& feed_a_;
    FeedSender& feed_b_;
    SnapshotBuilder builder_;

    std::atomic<bool> running_{false};
    std::atomic<uint32_t> last_incr_seq_{0};
    std::thread thread_;
    uint32_t cycle_count_ = 0;

    void run();
};

} // namespace cme::sim::market_data
