#pragma once

#include "../common/types.h"
#include "../config/exchange_config.h"
#include "../engine/engine_event.h"
#include "../instruments/instrument_manager.h"
#include "channel_publisher.h"
#include "../common/asio_compat.h"
#include <atomic>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace cme::sim::market_data {

/// Top-level MDP 3.0 market data publisher.
/// Owns one ChannelPublisher per channel and routes engine events to the
/// correct channel based on security-to-channel mapping. Manages background
/// threads for snapshot cycling and instrument definition replay.
class MarketDataPublisher {
public:
    MarketDataPublisher(const std::vector<config::ChannelConfig>& channels,
                        const InstrumentManager& instrument_mgr,
                        boost::asio::io_context& io_ctx);

    ~MarketDataPublisher();

    /// Process engine events and publish to appropriate channel feeds.
    void publishEvents(const std::vector<EngineEvent>& events);

    /// Start background threads (snapshot cycling, instrument def replay).
    void start();

    /// Stop all background threads.
    void stop();

    /// Get a specific channel publisher (nullptr if not found).
    ChannelPublisher* getChannelPublisher(int channel_id);

    /// Set a callback to provide book snapshots for snapshot cycling.
    void setBookSnapshotProvider(SnapshotCycler::BookSnapshotProvider provider);

private:
    std::unordered_map<int, std::unique_ptr<ChannelPublisher>> channel_publishers_;
    std::unordered_map<SecurityId, int> security_to_channel_;
    const InstrumentManager& instrument_mgr_;

    SnapshotCycler::BookSnapshotProvider book_provider_;
    std::atomic<bool> running_{false};
    std::thread instdef_thread_;

    int getChannelForSecurity(SecurityId sec_id) const;
    void instdefReplayLoop();
};

} // namespace cme::sim::market_data
