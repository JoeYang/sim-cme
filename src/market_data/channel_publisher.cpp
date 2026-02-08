#include "market_data/channel_publisher.h"
#include "../sbe/mdp3_messages.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace cme::sim::market_data {

ChannelPublisher::ChannelPublisher(int channel_id,
                                   const config::ChannelConfig& config,
                                   boost::asio::io_context& io_ctx)
    : channel_id_(channel_id)
    , incremental_feed_a_(io_ctx, config.incremental_feed.address_a,
                          config.incremental_feed.port_a)
    , incremental_feed_b_(io_ctx, config.incremental_feed.address_b,
                          config.incremental_feed.port_b)
    , snapshot_feed_a_(io_ctx, config.snapshot_feed.address_a,
                       config.snapshot_feed.port_a)
    , snapshot_feed_b_(io_ctx, config.snapshot_feed.address_b,
                       config.snapshot_feed.port_b)
    , instdef_feed_a_(io_ctx, config.instrument_def_feed.address_a,
                      config.instrument_def_feed.port_a)
    , instdef_feed_b_(io_ctx, config.instrument_def_feed.address_b,
                      config.instrument_def_feed.port_b) {
    spdlog::info("ChannelPublisher created for channel {}", channel_id);
}

void ChannelPublisher::publishIncrementalUpdates(
    const std::vector<EngineEvent>& events,
    Timestamp transact_time) {
    if (events.empty()) return;

    auto packet = incremental_builder_.buildIncrementalPacket(events, transact_time);
    if (packet.empty()) return;

    // Send on both redundant feeds with the same sequence number
    uint32_t seq = incremental_feed_a_.nextSeqNum();
    incremental_feed_a_.send(packet);
    incremental_feed_b_.sendWithSeqNum(seq, packet);

    // Update snapshot cycler with latest incremental seq num
    if (snapshot_cycler_) {
        snapshot_cycler_->setLastIncrementalSeqNum(seq);
    }
}

void ChannelPublisher::publishSecurityStatus(SecurityId security_id,
                                              SecurityTradingStatus status,
                                              Timestamp transact_time) {
    sbe::SecurityStatus30 msg;
    msg.transactTime = transact_time;
    msg.securityID = security_id;
    msg.securityTradingStatus = static_cast<uint8_t>(status);
    msg.matchEventIndicator =
        static_cast<uint8_t>(MatchEventIndicator::EndOfEvent);

    std::vector<uint8_t> buf(msg.encodedLength());
    msg.encode(reinterpret_cast<char*>(buf.data()), 0);

    uint32_t seq = incremental_feed_a_.nextSeqNum();
    incremental_feed_a_.send(buf);
    incremental_feed_b_.sendWithSeqNum(seq, buf);

    if (snapshot_cycler_) {
        snapshot_cycler_->setLastIncrementalSeqNum(seq);
    }
}

void ChannelPublisher::replayInstrumentDefinitions(
    const std::vector<Instrument>& instruments) {
    uint32_t tot = static_cast<uint32_t>(instruments.size());

    for (const auto& inst : instruments) {
        auto msg = instdef_builder_.buildDefinition(inst, tot);
        if (msg.empty()) continue;

        instdef_feed_a_.send(msg);
        instdef_feed_b_.send(msg);
    }

    spdlog::debug("Channel {} replayed {} instrument definitions",
                  channel_id_, instruments.size());
}

void ChannelPublisher::initSnapshotCycler(
    const std::vector<SecurityId>& instruments,
    SnapshotCycler::BookSnapshotProvider provider) {
    snapshot_cycler_ = std::make_unique<SnapshotCycler>(
        instruments, std::move(provider),
        snapshot_feed_a_, snapshot_feed_b_);
}

} // namespace cme::sim::market_data
