#include "market_data/snapshot_builder.h"
#include "../sbe/mdp3_messages.h"

namespace cme::sim::market_data {

std::vector<uint8_t> SnapshotBuilder::buildSnapshot(
    SecurityId security_id,
    uint32_t last_msg_seq_num_processed,
    uint32_t tot_num_reports,
    const std::vector<std::pair<Price, Quantity>>& bids,
    const std::vector<std::pair<Price, Quantity>>& asks,
    const std::vector<int>& bid_order_counts,
    const std::vector<int>& ask_order_counts,
    uint32_t rpt_seq,
    Timestamp transact_time) {

    sbe::SnapshotFullRefresh52 msg;
    msg.lastMsgSeqNumProcessed = last_msg_seq_num_processed;
    msg.totNumReports = tot_num_reports;
    msg.securityID = security_id;
    msg.rptSeq = rpt_seq;
    msg.transactTime = transact_time;
    msg.lastUpdateTime = transact_time;
    msg.tradeDate = 0;
    msg.mdSecurityTradingStatus = static_cast<uint8_t>(SecurityTradingStatus::Open);
    msg.highLimitPrice = Price::null().mantissa;
    msg.lowLimitPrice = Price::null().mantissa;
    msg.maxPriceVariation = Price::null().mantissa;

    // Add bid levels
    for (size_t i = 0; i < bids.size(); ++i) {
        sbe::SnapshotFullRefresh52::Entry entry{};
        entry.mdEntryPx = bids[i].first.mantissa;
        entry.mdEntrySize = bids[i].second;
        entry.numberOfOrders = (i < bid_order_counts.size())
                                   ? bid_order_counts[i]
                                   : 0;
        entry.mdPriceLevel = static_cast<uint8_t>(i + 1); // 1-based
        entry.mdEntryType = static_cast<char>(MDEntryType::Bid);
        msg.entries.push_back(entry);
    }

    // Add ask levels
    for (size_t i = 0; i < asks.size(); ++i) {
        sbe::SnapshotFullRefresh52::Entry entry{};
        entry.mdEntryPx = asks[i].first.mantissa;
        entry.mdEntrySize = asks[i].second;
        entry.numberOfOrders = (i < ask_order_counts.size())
                                   ? ask_order_counts[i]
                                   : 0;
        entry.mdPriceLevel = static_cast<uint8_t>(i + 1); // 1-based
        entry.mdEntryType = static_cast<char>(MDEntryType::Offer);
        msg.entries.push_back(entry);
    }

    std::vector<uint8_t> buf(msg.encodedLength());
    msg.encode(reinterpret_cast<char*>(buf.data()), 0);
    return buf;
}

} // namespace cme::sim::market_data
