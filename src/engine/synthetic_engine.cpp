#include "synthetic_engine.h"
#include "../sbe/message_header.h"
#include <algorithm>
#include <chrono>
#include <cstring>

namespace cme::sim {

// MDP 3.0 binary packet header: 4 bytes before SBE messages
// [2 bytes MsgSeqNum (uint32_t)] [2 bytes SendingTime offset -- actually 8+2 per CME spec]
// Simplified: first 12 bytes are MDP3 packet header (MsgSeqNum(4) + SendingTime(8))
static constexpr size_t MDP3_PACKET_HEADER_SIZE = 12;

SyntheticEngine::SyntheticEngine(const std::string& pcap_path, double speed_multiplier)
    : pcap_reader_(pcap_path)
    , speed_multiplier_(speed_multiplier) {
}

SyntheticEngine::~SyntheticEngine() {
    stopReplay();
}

Timestamp SyntheticEngine::nowNs() {
    auto now = std::chrono::steady_clock::now();
    return static_cast<Timestamp>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count());
}

// --------------------------------------------------------------------------
// IMatchingEngine interface
// --------------------------------------------------------------------------

std::vector<EngineEvent> SyntheticEngine::submitOrder(std::unique_ptr<Order> order) {
    std::vector<EngineEvent> events;

    // Assign order ID
    order->order_id = next_order_id_++;
    order->timestamp = nowNs();
    order->status = OrdStatus::New;

    SecurityId sec_id = order->security_id;
    OrderId oid = order->order_id;

    // Emit OrderAccepted
    OrderAccepted accepted{};
    accepted.order_id = order->order_id;
    accepted.cl_ord_id = order->cl_ord_id;
    accepted.session_uuid = order->session_uuid;
    accepted.security_id = order->security_id;
    accepted.side = order->side;
    accepted.price = order->price;
    accepted.quantity = order->quantity;
    accepted.order_type = order->order_type;
    accepted.time_in_force = order->time_in_force;
    events.push_back(accepted);

    // Check for immediate fill against current BBO
    BBO bbo;
    {
        std::lock_guard<std::mutex> lock(bbo_mutex_);
        auto it = bbos_.find(sec_id);
        if (it != bbos_.end()) {
            bbo = it->second;
        }
    }

    if (isMarketable(*order, bbo)) {
        // Determine fill price
        Price fill_price;
        if (order->side == Side::Buy) {
            fill_price = bbo.best_ask;
        } else {
            fill_price = bbo.best_bid;
        }

        Quantity fill_qty = order->remainingQty();
        events.push_back(generateFill(*order, fill_price, fill_qty));
    } else {
        // IOC/FOK orders that are not marketable get cancelled
        if (order->time_in_force == TimeInForce::IOC ||
            order->time_in_force == TimeInForce::FOK) {
            order->status = OrdStatus::Canceled;

            OrderCancelled cancelled{};
            cancelled.order_id = order->order_id;
            cancelled.cl_ord_id = order->cl_ord_id;
            cancelled.session_uuid = order->session_uuid;
            cancelled.security_id = order->security_id;
            cancelled.cum_qty = order->filled_qty;
            cancelled.ord_status = OrdStatus::Canceled;
            events.push_back(cancelled);
        } else {
            // Store as resting order
            std::lock_guard<std::mutex> lock(orders_mutex_);
            orders_by_security_[sec_id].push_back(oid);
            RestingOrder resting;
            resting.order = std::move(order);
            resting.submit_time = nowNs();
            resting_orders_.emplace(oid, std::move(resting));
        }
    }

    return events;
}

std::vector<EngineEvent> SyntheticEngine::cancelOrder(OrderId order_id, SecurityId security_id, uint64_t session_uuid) {
    std::vector<EngineEvent> events;
    std::lock_guard<std::mutex> lock(orders_mutex_);

    auto it = resting_orders_.find(order_id);
    if (it == resting_orders_.end()) {
        OrderCancelRejected rejected{};
        rejected.order_id = order_id;
        rejected.session_uuid = session_uuid;
        rejected.reject_reason_code = 1;
        rejected.reason = "Unknown order";
        events.push_back(rejected);
        return events;
    }

    Order& order = *it->second.order;
    order.status = OrdStatus::Canceled;

    OrderCancelled cancelled{};
    cancelled.order_id = order.order_id;
    cancelled.cl_ord_id = order.cl_ord_id;
    cancelled.session_uuid = order.session_uuid;
    cancelled.security_id = order.security_id;
    cancelled.cum_qty = order.filled_qty;
    cancelled.ord_status = OrdStatus::Canceled;
    events.push_back(cancelled);

    // Remove from orders_by_security_
    auto& sec_orders = orders_by_security_[security_id];
    sec_orders.erase(std::remove(sec_orders.begin(), sec_orders.end(), order_id), sec_orders.end());

    resting_orders_.erase(it);
    return events;
}

std::vector<EngineEvent> SyntheticEngine::modifyOrder(OrderId order_id, SecurityId security_id, Price new_price, Quantity new_qty, ClOrdId new_cl_ord_id) {
    std::vector<EngineEvent> events;
    std::lock_guard<std::mutex> lock(orders_mutex_);

    auto it = resting_orders_.find(order_id);
    if (it == resting_orders_.end()) {
        OrderCancelRejected rejected{};
        rejected.order_id = order_id;
        rejected.reject_reason_code = 1;
        rejected.reason = "Unknown order";
        events.push_back(rejected);
        return events;
    }

    Order& order = *it->second.order;
    order.price = new_price;
    order.quantity = new_qty;
    order.cl_ord_id = new_cl_ord_id;
    order.status = OrdStatus::Replaced;

    OrderModified modified{};
    modified.order_id = order.order_id;
    modified.cl_ord_id = order.cl_ord_id;
    modified.session_uuid = order.session_uuid;
    modified.security_id = order.security_id;
    modified.new_price = new_price;
    modified.new_qty = new_qty;
    modified.cum_qty = order.filled_qty;
    modified.leaves_qty = order.remainingQty();
    events.push_back(modified);

    return events;
}

// --------------------------------------------------------------------------
// Replay control
// --------------------------------------------------------------------------

void SyntheticEngine::startReplay() {
    if (running_.load()) return;

    if (!pcap_reader_.isOpen()) {
        if (!pcap_reader_.open()) {
            return;
        }
    }

    running_.store(true);
    replay_thread_ = std::thread(&SyntheticEngine::replayLoop, this);
}

void SyntheticEngine::stopReplay() {
    running_.store(false);
    if (replay_thread_.joinable()) {
        replay_thread_.join();
    }
}

void SyntheticEngine::setMarketDataCallback(MarketDataCallback cb) {
    md_callback_ = std::move(cb);
}

void SyntheticEngine::setFillProbability(double prob) {
    fill_probability_ = std::max(0.0, std::min(1.0, prob));
}

void SyntheticEngine::setFillLatencyNs(uint64_t latency_ns) {
    fill_latency_ns_ = latency_ns;
}

BBO SyntheticEngine::getBBO(SecurityId security_id) const {
    std::lock_guard<std::mutex> lock(bbo_mutex_);
    auto it = bbos_.find(security_id);
    if (it != bbos_.end()) {
        return it->second;
    }
    return BBO{};
}

// --------------------------------------------------------------------------
// Replay loop
// --------------------------------------------------------------------------

void SyntheticEngine::replayLoop() {
    PcapPacket packet;
    uint64_t first_pcap_ts = 0;
    auto replay_start = std::chrono::steady_clock::now();

    while (running_.load() && pcap_reader_.readNext(packet)) {
        if (first_pcap_ts == 0) {
            first_pcap_ts = packet.timestamp_us;
        }

        // Pace replay according to original timestamps and speed multiplier
        if (speed_multiplier_ > 0.0) {
            uint64_t pcap_elapsed_us = packet.timestamp_us - first_pcap_ts;
            auto target_elapsed = std::chrono::microseconds(
                static_cast<uint64_t>(pcap_elapsed_us / speed_multiplier_));
            auto now = std::chrono::steady_clock::now();
            auto actual_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                now - replay_start);

            if (target_elapsed > actual_elapsed) {
                auto sleep_time = target_elapsed - actual_elapsed;
                std::this_thread::sleep_for(sleep_time);
            }
        }

        if (!running_.load()) break;

        // Forward raw packet to market data callback
        if (md_callback_ && !packet.data.empty()) {
            md_callback_(packet.data.data(), packet.data.size());
        }

        // Process for BBO updates and fill checks
        if (!packet.data.empty()) {
            processReplayedPacket(packet.data.data(), packet.data.size());
        }
    }

    running_.store(false);
}

// --------------------------------------------------------------------------
// MDP3 packet processing
// --------------------------------------------------------------------------

void SyntheticEngine::processReplayedPacket(const char* data, size_t len) {
    // MDP 3.0 binary packet format:
    // [4 bytes MsgSeqNum] [8 bytes SendingTime] [SBE messages...]
    if (len < MDP3_PACKET_HEADER_SIZE) return;

    size_t offset = MDP3_PACKET_HEADER_SIZE;

    // Iterate over SBE messages within the packet
    while (offset + sbe::MessageHeader::SIZE <= len) {
        uint16_t template_id = sbe::MessageHeader::decodeTemplateId(data + offset);
        uint16_t block_length = sbe::MessageHeader::decodeBlockLength(data + offset);

        if (template_id == sbe::MDIncrementalRefreshBook46::TEMPLATE_ID) {
            sbe::MDIncrementalRefreshBook46 msg;
            msg.decode(data, offset);
            updateBBO(msg);

            // Advance past the message
            offset += msg.encodedLength();
        } else if (template_id == sbe::MDIncrementalRefreshTradeSummary48::TEMPLATE_ID) {
            sbe::MDIncrementalRefreshTradeSummary48 msg;
            msg.decode(data, offset);

            // For each trade entry, check resting orders
            for (const auto& entry : msg.mdEntries) {
                Price trade_price{entry.mdEntryPx};
                Quantity trade_qty = entry.mdEntrySize;
                SecurityId sec_id = entry.securityID;
                Side aggressor = (entry.aggressorSide == 1) ? Side::Buy : Side::Sell;

                auto fill_events = checkFillsOnTrade(sec_id, trade_price, trade_qty, aggressor);
                if (!fill_events.empty()) {
                    std::lock_guard<std::mutex> lock(event_mutex_);
                    for (auto& ev : fill_events) {
                        pending_events_.push_back(std::move(ev));
                    }
                }
            }

            offset += msg.encodedLength();
        } else {
            // Skip unknown message: header + block length
            // We need to be careful -- some messages have repeating groups.
            // For safety, skip using block_length plus header. If the message has
            // repeating groups we can't easily determine the total length without
            // decoding, so we just skip the root block and hope subsequent messages
            // align. This is a best-effort approach for messages we don't handle.
            offset += sbe::MessageHeader::SIZE + block_length;
        }
    }
}

void SyntheticEngine::updateBBO(const sbe::MDIncrementalRefreshBook46& msg) {
    std::lock_guard<std::mutex> lock(bbo_mutex_);

    for (const auto& entry : msg.entries) {
        SecurityId sec_id = entry.securityID;
        auto action = static_cast<MDUpdateAction>(entry.mdUpdateAction);
        Price price{entry.mdEntryPx};
        Quantity size = entry.mdEntrySize;

        BBO& bbo = bbos_[sec_id];

        if (entry.mdEntryType == '0') {
            // Bid
            if (entry.mdPriceLevel == 1) {
                if (action == MDUpdateAction::New || action == MDUpdateAction::Change ||
                    action == MDUpdateAction::Overlay) {
                    bbo.best_bid = price;
                    bbo.bid_size = size;
                } else if (action == MDUpdateAction::Delete) {
                    bbo.best_bid = Price::null();
                    bbo.bid_size = 0;
                }
            }
        } else if (entry.mdEntryType == '1') {
            // Offer
            if (entry.mdPriceLevel == 1) {
                if (action == MDUpdateAction::New || action == MDUpdateAction::Change ||
                    action == MDUpdateAction::Overlay) {
                    bbo.best_ask = price;
                    bbo.ask_size = size;
                } else if (action == MDUpdateAction::Delete) {
                    bbo.best_ask = Price::null();
                    bbo.ask_size = 0;
                }
            }
        }
    }
}

std::vector<EngineEvent> SyntheticEngine::checkFillsOnTrade(
    SecurityId security_id, Price trade_price, Quantity trade_qty, Side aggressor_side) {

    std::vector<EngineEvent> events;
    std::lock_guard<std::mutex> lock(orders_mutex_);

    auto sec_it = orders_by_security_.find(security_id);
    if (sec_it == orders_by_security_.end()) return events;

    auto& order_ids = sec_it->second;
    std::vector<OrderId> filled_ids;

    for (auto oid : order_ids) {
        auto oit = resting_orders_.find(oid);
        if (oit == resting_orders_.end()) continue;

        Order& order = *oit->second.order;

        // Check if the trade price crosses the resting order
        bool should_fill_price = false;
        if (order.side == Side::Buy && trade_price <= order.price) {
            should_fill_price = true;
        } else if (order.side == Side::Sell && trade_price >= order.price) {
            should_fill_price = true;
        }

        if (!should_fill_price) continue;
        if (!shouldFill()) continue;

        // Fill at the trade price for the minimum of order remaining and trade qty
        Quantity fill_qty = std::min(order.remainingQty(), trade_qty);
        if (fill_qty <= 0) continue;

        events.push_back(generateFill(order, trade_price, fill_qty));

        if (order.isFullyFilled()) {
            filled_ids.push_back(oid);
        }
    }

    // Remove fully filled orders
    for (auto oid : filled_ids) {
        resting_orders_.erase(oid);
        order_ids.erase(std::remove(order_ids.begin(), order_ids.end(), oid), order_ids.end());
    }

    return events;
}

bool SyntheticEngine::isMarketable(const Order& order, const BBO& bbo) const {
    if (order.order_type == OrderType::Market) {
        // Market orders are always marketable if there's a BBO
        if (order.side == Side::Buy) return !bbo.best_ask.isNull() && bbo.ask_size > 0;
        return !bbo.best_bid.isNull() && bbo.bid_size > 0;
    }

    if (order.side == Side::Buy) {
        return !bbo.best_ask.isNull() && bbo.ask_size > 0 && order.price >= bbo.best_ask;
    } else {
        return !bbo.best_bid.isNull() && bbo.bid_size > 0 && order.price <= bbo.best_bid;
    }
}

EngineEvent SyntheticEngine::generateFill(Order& order, Price fill_price, Quantity fill_qty) {
    order.filled_qty += fill_qty;
    if (order.isFullyFilled()) {
        order.status = OrdStatus::Filled;
    } else {
        order.status = OrdStatus::PartiallyFilled;
    }

    OrderFilled fill{};
    fill.trade_id = next_trade_id_++;
    fill.security_id = order.security_id;
    fill.trade_price = fill_price;
    fill.trade_qty = fill_qty;
    fill.aggressor_side = order.side;

    // In synthetic mode, the order is both maker and taker conceptually,
    // but we report it as the taker side since we're filling against replayed data.
    fill.taker_order_id = order.order_id;
    fill.taker_cl_ord_id = order.cl_ord_id;
    fill.taker_session_uuid = order.session_uuid;
    fill.taker_cum_qty = order.filled_qty;
    fill.taker_leaves_qty = order.remainingQty();
    fill.taker_ord_status = order.status;

    // Maker side is synthetic (the replayed market)
    fill.maker_order_id = 0;
    fill.maker_cl_ord_id = "MARKET";
    fill.maker_session_uuid = 0;
    fill.maker_cum_qty = fill_qty;
    fill.maker_leaves_qty = 0;
    fill.maker_ord_status = OrdStatus::Filled;

    return fill;
}

bool SyntheticEngine::shouldFill() const {
    if (fill_probability_ >= 1.0) return true;
    if (fill_probability_ <= 0.0) return false;

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng_) < fill_probability_;
}

} // namespace cme::sim
