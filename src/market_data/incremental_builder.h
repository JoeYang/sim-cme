#pragma once

#include "../common/types.h"
#include "../engine/engine_event.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace cme::sim::market_data {

/// Builds MDP 3.0 incremental refresh messages from engine events.
class IncrementalBuilder {
public:
    /// Build MDIncrementalRefreshBook46 from BookUpdate events.
    /// Returns SBE-encoded message bytes (without packet header).
    std::vector<uint8_t> buildBookRefresh(
        const std::vector<EngineEvent>& book_updates,
        Timestamp transact_time);

    /// Build MDIncrementalRefreshTradeSummary48 from OrderFilled events.
    std::vector<uint8_t> buildTradeSummary(
        const std::vector<EngineEvent>& trade_events,
        Timestamp transact_time);

    /// Build a combined SBE payload containing both book refresh and trade
    /// summary messages for a batch of events. Messages are concatenated
    /// back-to-back so they can be packed into a single MDP3 packet.
    std::vector<uint8_t> buildIncrementalPacket(
        const std::vector<EngineEvent>& events,
        Timestamp transact_time);

private:
    std::unordered_map<SecurityId, uint32_t> rpt_seqs_;
    uint32_t getNextRptSeq(SecurityId sec_id);
};

} // namespace cme::sim::market_data
