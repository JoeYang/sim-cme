#include "market_data/incremental_builder.h"
#include "../sbe/mdp3_messages.h"
#include <algorithm>

namespace cme::sim::market_data {

uint32_t IncrementalBuilder::getNextRptSeq(SecurityId sec_id) {
    return ++rpt_seqs_[sec_id];
}

std::vector<uint8_t> IncrementalBuilder::buildBookRefresh(
    const std::vector<EngineEvent>& book_updates,
    Timestamp transact_time) {

    sbe::MDIncrementalRefreshBook46 msg;
    msg.transactTime = transact_time;
    msg.matchEventIndicator =
        static_cast<uint8_t>(MatchEventIndicator::LastQuoteMsg) |
        static_cast<uint8_t>(MatchEventIndicator::EndOfEvent);

    for (const auto& ev : book_updates) {
        const auto* bu = std::get_if<BookUpdate>(&ev);
        if (!bu) continue;

        sbe::MDIncrementalRefreshBook46::Entry entry{};
        entry.mdEntryPx = bu->price.mantissa;
        entry.mdEntrySize = bu->new_qty;
        entry.securityID = bu->security_id;
        entry.rptSeq = bu->rpt_seq != 0 ? bu->rpt_seq : getNextRptSeq(bu->security_id);
        entry.numberOfOrders = bu->new_order_count;
        entry.mdPriceLevel = static_cast<uint8_t>(bu->price_level_index);
        entry.mdUpdateAction = static_cast<uint8_t>(bu->update_action);
        entry.mdEntryType = (bu->side == Side::Buy)
                                ? static_cast<char>(MDEntryType::Bid)
                                : static_cast<char>(MDEntryType::Offer);
        msg.entries.push_back(entry);
    }

    if (msg.entries.empty()) return {};

    std::vector<uint8_t> buf(msg.encodedLength());
    msg.encode(reinterpret_cast<char*>(buf.data()), 0);
    return buf;
}

std::vector<uint8_t> IncrementalBuilder::buildTradeSummary(
    const std::vector<EngineEvent>& trade_events,
    Timestamp transact_time) {

    sbe::MDIncrementalRefreshTradeSummary48 msg;
    msg.transactTime = transact_time;
    msg.matchEventIndicator =
        static_cast<uint8_t>(MatchEventIndicator::LastTradeMsg) |
        static_cast<uint8_t>(MatchEventIndicator::LastVolumeMsg) |
        static_cast<uint8_t>(MatchEventIndicator::EndOfEvent);

    for (const auto& ev : trade_events) {
        const auto* fill = std::get_if<OrderFilled>(&ev);
        if (!fill) continue;

        sbe::MDIncrementalRefreshTradeSummary48::MDEntry entry{};
        entry.mdEntryPx = fill->trade_price.mantissa;
        entry.mdEntrySize = fill->trade_qty;
        entry.securityID = fill->security_id;
        entry.rptSeq = getNextRptSeq(fill->security_id);
        entry.numberOfOrders = 2; // maker + taker
        entry.aggressorSide = static_cast<uint8_t>(fill->aggressor_side);
        entry.mdUpdateAction = static_cast<uint8_t>(MDUpdateAction::New);
        msg.mdEntries.push_back(entry);

        // OrderID entries for both sides of the trade
        sbe::MDIncrementalRefreshTradeSummary48::OrderIDEntry maker_oid{};
        maker_oid.orderID = fill->maker_order_id;
        maker_oid.lastQty = fill->trade_qty;
        msg.orderIDEntries.push_back(maker_oid);

        sbe::MDIncrementalRefreshTradeSummary48::OrderIDEntry taker_oid{};
        taker_oid.orderID = fill->taker_order_id;
        taker_oid.lastQty = fill->trade_qty;
        msg.orderIDEntries.push_back(taker_oid);
    }

    if (msg.mdEntries.empty()) return {};

    std::vector<uint8_t> buf(msg.encodedLength());
    msg.encode(reinterpret_cast<char*>(buf.data()), 0);
    return buf;
}

std::vector<uint8_t> IncrementalBuilder::buildIncrementalPacket(
    const std::vector<EngineEvent>& events,
    Timestamp transact_time) {

    // Separate events into book updates and trades
    std::vector<EngineEvent> book_updates;
    std::vector<EngineEvent> trades;

    for (const auto& ev : events) {
        if (std::holds_alternative<BookUpdate>(ev)) {
            book_updates.push_back(ev);
        } else if (std::holds_alternative<OrderFilled>(ev)) {
            trades.push_back(ev);
        }
    }

    // Build individual SBE messages
    auto book_msg = buildBookRefresh(book_updates, transact_time);
    auto trade_msg = buildTradeSummary(trades, transact_time);

    // Concatenate messages into a single packet payload
    std::vector<uint8_t> packet;
    packet.reserve(book_msg.size() + trade_msg.size());
    packet.insert(packet.end(), book_msg.begin(), book_msg.end());
    packet.insert(packet.end(), trade_msg.begin(), trade_msg.end());

    return packet;
}

} // namespace cme::sim::market_data
