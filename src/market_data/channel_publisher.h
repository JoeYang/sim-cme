#pragma once

#include "../common/types.h"
#include "../config/exchange_config.h"
#include "../engine/engine_event.h"
#include "../instruments/instrument.h"
#include "feed_sender.h"
#include "incremental_builder.h"
#include "instrument_def_builder.h"
#include "snapshot_builder.h"
#include "snapshot_cycler.h"
#include "../common/asio_compat.h"
#include <memory>
#include <vector>

namespace cme::sim::market_data {

/// Per-channel publisher that manages all six feeds (incremental A+B,
/// snapshot A+B, instrument-definition A+B) for one MDP 3.0 channel.
class ChannelPublisher {
public:
    ChannelPublisher(int channel_id,
                     const config::ChannelConfig& config,
                     boost::asio::io_context& io_ctx);

    /// Publish incremental updates from engine events.
    /// Events are encoded into MDP 3.0 messages and sent on both feeds.
    void publishIncrementalUpdates(const std::vector<EngineEvent>& events,
                                   Timestamp transact_time);

    /// Publish a SecurityStatus30 message for a trading-status change.
    void publishSecurityStatus(SecurityId security_id,
                               SecurityTradingStatus status,
                               Timestamp transact_time);

    /// Replay instrument definitions for all instruments on this channel.
    void replayInstrumentDefinitions(const std::vector<Instrument>& instruments);

    /// Access snapshot cycler to configure and start/stop snapshot cycling.
    SnapshotCycler& snapshotCycler() { return *snapshot_cycler_; }

    /// Initialize the snapshot cycler with instruments and book provider.
    void initSnapshotCycler(const std::vector<SecurityId>& instruments,
                            SnapshotCycler::BookSnapshotProvider provider);

    int channelId() const { return channel_id_; }

    /// Get the current incremental feed sequence number (Feed A).
    uint32_t currentIncrementalSeqNum() const {
        return incremental_feed_a_.nextSeqNum() - 1;
    }

private:
    int channel_id_;

    // Redundant feed pairs
    FeedSender incremental_feed_a_;
    FeedSender incremental_feed_b_;
    FeedSender snapshot_feed_a_;
    FeedSender snapshot_feed_b_;
    FeedSender instdef_feed_a_;
    FeedSender instdef_feed_b_;

    // Message builders
    IncrementalBuilder incremental_builder_;
    InstrumentDefBuilder instdef_builder_;

    // Snapshot cycler (created on demand via initSnapshotCycler)
    std::unique_ptr<SnapshotCycler> snapshot_cycler_;
};

} // namespace cme::sim::market_data
