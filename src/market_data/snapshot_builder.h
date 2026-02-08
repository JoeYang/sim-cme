#pragma once

#include "../common/types.h"
#include <cstdint>
#include <utility>
#include <vector>

namespace cme::sim::market_data {

/// Builds SnapshotFullRefresh52 messages for MDP 3.0 snapshot feeds.
class SnapshotBuilder {
public:
    /// Build a full order-book snapshot for one instrument.
    /// @param security_id             Instrument identifier
    /// @param last_msg_seq_num_processed  Last incremental seq num at time of snapshot
    /// @param tot_num_reports         Total number of snapshot messages in this cycle
    /// @param bids                    Bid levels: (price, size) best to worst
    /// @param asks                    Ask levels: (price, size) best to worst
    /// @param bid_order_counts        Number of orders at each bid level
    /// @param ask_order_counts        Number of orders at each ask level
    /// @param rpt_seq                 Report sequence number for this snapshot
    /// @param transact_time           Timestamp in nanoseconds since epoch
    std::vector<uint8_t> buildSnapshot(
        SecurityId security_id,
        uint32_t last_msg_seq_num_processed,
        uint32_t tot_num_reports,
        const std::vector<std::pair<Price, Quantity>>& bids,
        const std::vector<std::pair<Price, Quantity>>& asks,
        const std::vector<int>& bid_order_counts,
        const std::vector<int>& ask_order_counts,
        uint32_t rpt_seq,
        Timestamp transact_time);
};

} // namespace cme::sim::market_data
