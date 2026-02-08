#include "market_data/market_data_publisher.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace cme::sim::market_data {

MarketDataPublisher::MarketDataPublisher(
    const std::vector<config::ChannelConfig>& channels,
    const InstrumentManager& instrument_mgr,
    boost::asio::io_context& io_ctx)
    : instrument_mgr_(instrument_mgr) {

    // Build security-to-channel mapping from instrument manager
    for (const auto& inst : instrument_mgr.getAllInstruments()) {
        security_to_channel_[inst.security_id] = inst.channel_id;
    }

    // Create one ChannelPublisher per channel
    for (const auto& ch_cfg : channels) {
        auto pub = std::make_unique<ChannelPublisher>(
            ch_cfg.channel_id, ch_cfg, io_ctx);
        channel_publishers_[ch_cfg.channel_id] = std::move(pub);
    }

    spdlog::info("MarketDataPublisher created with {} channels, {} instruments",
                 channels.size(), security_to_channel_.size());
}

MarketDataPublisher::~MarketDataPublisher() {
    stop();
}

void MarketDataPublisher::publishEvents(const std::vector<EngineEvent>& events) {
    if (events.empty()) return;

    Timestamp ts = FeedSender::now();

    // Group events by channel
    std::unordered_map<int, std::vector<EngineEvent>> by_channel;

    for (const auto& ev : events) {
        SecurityId sec_id = 0;

        if (const auto* bu = std::get_if<BookUpdate>(&ev)) {
            sec_id = bu->security_id;
        } else if (const auto* fill = std::get_if<OrderFilled>(&ev)) {
            sec_id = fill->security_id;
        } else {
            // Other event types (OrderAccepted, etc.) are session-level;
            // they are not published on the market data feed.
            continue;
        }

        int channel = getChannelForSecurity(sec_id);
        if (channel != 0) {
            by_channel[channel].push_back(ev);
        }
    }

    // Publish each channel's events
    for (auto& [channel_id, ch_events] : by_channel) {
        auto it = channel_publishers_.find(channel_id);
        if (it != channel_publishers_.end()) {
            it->second->publishIncrementalUpdates(ch_events, ts);
        }
    }
}

void MarketDataPublisher::start() {
    if (running_.exchange(true)) return;

    // Initialize snapshot cyclers for each channel
    if (book_provider_) {
        for (auto& [ch_id, pub] : channel_publishers_) {
            auto instruments = instrument_mgr_.getInstrumentsByChannel(ch_id);
            std::vector<SecurityId> sec_ids;
            sec_ids.reserve(instruments.size());
            for (const auto* inst : instruments) {
                sec_ids.push_back(inst->security_id);
            }
            pub->initSnapshotCycler(sec_ids, book_provider_);
            pub->snapshotCycler().start();
        }
    }

    // Start instrument definition replay thread
    instdef_thread_ = std::thread(&MarketDataPublisher::instdefReplayLoop, this);

    spdlog::info("MarketDataPublisher started");
}

void MarketDataPublisher::stop() {
    if (!running_.exchange(false)) return;

    // Stop snapshot cyclers (only if they were initialized)
    if (book_provider_) {
        for (auto& [ch_id, pub] : channel_publishers_) {
            pub->snapshotCycler().stop();
        }
    }

    // Stop instrument def replay
    if (instdef_thread_.joinable()) {
        instdef_thread_.join();
    }

    spdlog::info("MarketDataPublisher stopped");
}

ChannelPublisher* MarketDataPublisher::getChannelPublisher(int channel_id) {
    auto it = channel_publishers_.find(channel_id);
    return (it != channel_publishers_.end()) ? it->second.get() : nullptr;
}

void MarketDataPublisher::setBookSnapshotProvider(
    SnapshotCycler::BookSnapshotProvider provider) {
    book_provider_ = std::move(provider);
}

int MarketDataPublisher::getChannelForSecurity(SecurityId sec_id) const {
    auto it = security_to_channel_.find(sec_id);
    return (it != security_to_channel_.end()) ? it->second : 0;
}

void MarketDataPublisher::instdefReplayLoop() {
    spdlog::info("Instrument definition replay thread started");

    while (running_.load(std::memory_order_relaxed)) {
        // Replay all instrument definitions per channel
        for (auto& [ch_id, pub] : channel_publishers_) {
            auto instruments = instrument_mgr_.getInstrumentsByChannel(ch_id);
            std::vector<Instrument> inst_copies;
            inst_copies.reserve(instruments.size());
            for (const auto* inst : instruments) {
                inst_copies.push_back(*inst);
            }
            pub->replayInstrumentDefinitions(inst_copies);
        }

        // CME replays instrument definitions in a continuous loop,
        // typically cycling every few seconds.
        for (int i = 0; i < 50 && running_.load(std::memory_order_relaxed); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    spdlog::info("Instrument definition replay thread stopped");
}

} // namespace cme::sim::market_data
