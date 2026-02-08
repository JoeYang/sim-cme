#include "market_data/snapshot_cycler.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace cme::sim::market_data {

SnapshotCycler::SnapshotCycler(const std::vector<SecurityId>& instruments,
                               BookSnapshotProvider provider,
                               FeedSender& feed_a,
                               FeedSender& feed_b)
    : instruments_(instruments)
    , provider_(std::move(provider))
    , feed_a_(feed_a)
    , feed_b_(feed_b) {}

void SnapshotCycler::runCycle(uint32_t last_incremental_seq_num) {
    uint32_t tot_num_reports = static_cast<uint32_t>(instruments_.size());

    for (const auto& sec_id : instruments_) {
        std::vector<std::pair<Price, Quantity>> bids;
        std::vector<std::pair<Price, Quantity>> asks;
        std::vector<int> bid_counts;
        std::vector<int> ask_counts;

        // Get current book state from the matching engine
        provider_(sec_id, bids, asks, bid_counts, ask_counts);

        uint64_t ts = FeedSender::now();
        auto snapshot = builder_.buildSnapshot(
            sec_id,
            last_incremental_seq_num,
            tot_num_reports,
            bids, asks,
            bid_counts, ask_counts,
            ++cycle_count_,
            ts);

        if (!snapshot.empty()) {
            feed_a_.send(snapshot);
            feed_b_.send(snapshot);
        }
    }
}

void SnapshotCycler::start() {
    if (running_.exchange(true)) return; // already running
    thread_ = std::thread(&SnapshotCycler::run, this);
    spdlog::info("SnapshotCycler started for {} instruments", instruments_.size());
}

void SnapshotCycler::stop() {
    running_.store(false, std::memory_order_relaxed);
    if (thread_.joinable()) {
        thread_.join();
    }
    spdlog::info("SnapshotCycler stopped");
}

void SnapshotCycler::run() {
    while (running_.load(std::memory_order_relaxed)) {
        uint32_t seq = last_incr_seq_.load(std::memory_order_relaxed);
        runCycle(seq);

        // Sleep between cycles to avoid spinning. CME typically cycles
        // snapshots at roughly once per second.
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace cme::sim::market_data
