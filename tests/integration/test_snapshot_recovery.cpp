#include <gtest/gtest.h>
#include "engine/order_book.h"
#include "engine/order.h"
#include "engine/engine_event.h"
#include "sbe/mdp3_messages.h"
#include "common/types.h"
#include <memory>
#include <vector>
#include <algorithm>

using namespace cme::sim;
using namespace cme::sim::sbe;

// Integration test: Verify that we can build a SnapshotFullRefresh52
// from the current book state, and that it correctly represents the
// bid/ask levels from the matching engine.

class SnapshotRecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
        book = std::make_unique<OrderBook>(12345);
    }

    Order* makeOrder(OrderId id, Side side, double price, Quantity qty) {
        auto order = std::make_unique<Order>();
        order->order_id = id;
        order->security_id = 12345;
        order->side = side;
        order->order_type = OrderType::Limit;
        order->time_in_force = TimeInForce::Day;
        order->price = Price::fromDouble(price);
        order->quantity = qty;
        order->cl_ord_id = "CLO" + std::to_string(id);
        order->session_uuid = 100;
        order->timestamp = id;
        Order* raw = order.get();
        owned_orders_.push_back(std::move(order));
        return raw;
    }

    // Build a SnapshotFullRefresh52 from the current book state,
    // tracking the last rpt_seq from BookUpdate events.
    SnapshotFullRefresh52 buildSnapshot(uint32_t last_msg_seq_num) {
        SnapshotFullRefresh52 snap;
        snap.lastMsgSeqNumProcessed = last_msg_seq_num;
        snap.totNumReports = 1;
        snap.securityID = 12345;
        snap.rptSeq = last_rpt_seq_;
        snap.transactTime = 1000000000ULL;
        snap.lastUpdateTime = 1000000000ULL;
        snap.tradeDate = 20250315;
        snap.mdSecurityTradingStatus = 17; // Open

        // Add bid levels
        int level_idx = 1;
        for (const auto& [price, level] : book->bidLevels()) {
            SnapshotFullRefresh52::Entry entry;
            entry.mdEntryPx = price.mantissa;
            entry.mdEntrySize = level.total_quantity;
            entry.numberOfOrders = level.order_count;
            entry.mdPriceLevel = static_cast<uint8_t>(level_idx);
            entry.mdEntryType = '0'; // Bid
            snap.entries.push_back(entry);
            ++level_idx;
        }

        // Add ask levels
        level_idx = 1;
        for (const auto& [price, level] : book->askLevels()) {
            SnapshotFullRefresh52::Entry entry;
            entry.mdEntryPx = price.mantissa;
            entry.mdEntrySize = level.total_quantity;
            entry.numberOfOrders = level.order_count;
            entry.mdPriceLevel = static_cast<uint8_t>(level_idx);
            entry.mdEntryType = '1'; // Offer
            snap.entries.push_back(entry);
            ++level_idx;
        }

        return snap;
    }

    // Track latest rpt_seq from all BookUpdate events
    void trackRptSeq(const std::vector<EngineEvent>& events) {
        for (const auto& ev : events) {
            if (std::holds_alternative<BookUpdate>(ev)) {
                last_rpt_seq_ = std::get<BookUpdate>(ev).rpt_seq;
            }
        }
    }

    std::unique_ptr<OrderBook> book;
    std::vector<std::unique_ptr<Order>> owned_orders_;
    uint32_t last_rpt_seq_ = 0;
};

// ---------------------------------------------------------------------------
// Test: Snapshot matches book state after building order book
// ---------------------------------------------------------------------------
TEST_F(SnapshotRecoveryTest, SnapshotMatchesBookState) {
    // Build a book with 3 bid and 3 ask levels
    auto e1 = book->addOrder(makeOrder(1, Side::Buy, 5000.00, 10));
    trackRptSeq(e1);
    auto e2 = book->addOrder(makeOrder(2, Side::Buy, 4999.75, 20));
    trackRptSeq(e2);
    auto e3 = book->addOrder(makeOrder(3, Side::Buy, 4999.50, 15));
    trackRptSeq(e3);
    auto e4 = book->addOrder(makeOrder(4, Side::Sell, 5000.25, 10));
    trackRptSeq(e4);
    auto e5 = book->addOrder(makeOrder(5, Side::Sell, 5000.50, 20));
    trackRptSeq(e5);
    auto e6 = book->addOrder(makeOrder(6, Side::Sell, 5000.75, 15));
    trackRptSeq(e6);

    auto snap = buildSnapshot(100);

    // Should have 6 entries: 3 bid + 3 ask
    ASSERT_EQ(snap.entries.size(), 6u);
    EXPECT_EQ(snap.lastMsgSeqNumProcessed, 100u);
    EXPECT_EQ(snap.securityID, 12345);

    // Verify bids (descending price: 5000.00, 4999.75, 4999.50)
    EXPECT_EQ(snap.entries[0].mdEntryType, '0'); // Bid
    EXPECT_EQ(snap.entries[0].mdEntryPx, Price::fromDouble(5000.00).mantissa);
    EXPECT_EQ(snap.entries[0].mdEntrySize, 10);
    EXPECT_EQ(snap.entries[0].mdPriceLevel, 1);

    EXPECT_EQ(snap.entries[1].mdEntryPx, Price::fromDouble(4999.75).mantissa);
    EXPECT_EQ(snap.entries[1].mdEntrySize, 20);
    EXPECT_EQ(snap.entries[1].mdPriceLevel, 2);

    EXPECT_EQ(snap.entries[2].mdEntryPx, Price::fromDouble(4999.50).mantissa);
    EXPECT_EQ(snap.entries[2].mdEntrySize, 15);
    EXPECT_EQ(snap.entries[2].mdPriceLevel, 3);

    // Verify asks (ascending price: 5000.25, 5000.50, 5000.75)
    EXPECT_EQ(snap.entries[3].mdEntryType, '1'); // Offer
    EXPECT_EQ(snap.entries[3].mdEntryPx, Price::fromDouble(5000.25).mantissa);
    EXPECT_EQ(snap.entries[3].mdEntrySize, 10);
    EXPECT_EQ(snap.entries[3].mdPriceLevel, 1);

    EXPECT_EQ(snap.entries[4].mdEntryPx, Price::fromDouble(5000.50).mantissa);
    EXPECT_EQ(snap.entries[4].mdEntrySize, 20);
    EXPECT_EQ(snap.entries[4].mdPriceLevel, 2);

    EXPECT_EQ(snap.entries[5].mdEntryPx, Price::fromDouble(5000.75).mantissa);
    EXPECT_EQ(snap.entries[5].mdEntrySize, 15);
    EXPECT_EQ(snap.entries[5].mdPriceLevel, 3);
}

// ---------------------------------------------------------------------------
// Test: Snapshot encode/decode roundtrip preserves all fields
// ---------------------------------------------------------------------------
TEST_F(SnapshotRecoveryTest, SnapshotEncodeDecodeRoundtrip) {
    auto e1 = book->addOrder(makeOrder(1, Side::Buy, 5000.00, 10));
    trackRptSeq(e1);
    auto e2 = book->addOrder(makeOrder(2, Side::Sell, 5000.25, 5));
    trackRptSeq(e2);

    auto snap = buildSnapshot(50);

    // Encode
    char buf[1024];
    size_t len = snap.encode(buf, 0);

    // Decode
    SnapshotFullRefresh52 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.lastMsgSeqNumProcessed, 50u);
    EXPECT_EQ(decoded.securityID, 12345);
    EXPECT_EQ(decoded.rptSeq, last_rpt_seq_);
    ASSERT_EQ(decoded.entries.size(), 2u);

    EXPECT_EQ(decoded.entries[0].mdEntryPx, Price::fromDouble(5000.00).mantissa);
    EXPECT_EQ(decoded.entries[0].mdEntrySize, 10);
    EXPECT_EQ(decoded.entries[0].mdEntryType, '0');

    EXPECT_EQ(decoded.entries[1].mdEntryPx, Price::fromDouble(5000.25).mantissa);
    EXPECT_EQ(decoded.entries[1].mdEntrySize, 5);
    EXPECT_EQ(decoded.entries[1].mdEntryType, '1');
    EXPECT_EQ(len, snap.encodedLength());
}

// ---------------------------------------------------------------------------
// Test: Snapshot after trade reflects reduced quantities
// ---------------------------------------------------------------------------
TEST_F(SnapshotRecoveryTest, SnapshotAfterTradeReflectsReducedQty) {
    auto e1 = book->addOrder(makeOrder(1, Side::Sell, 5000.25, 10));
    trackRptSeq(e1);
    auto e2 = book->addOrder(makeOrder(2, Side::Sell, 5000.50, 10));
    trackRptSeq(e2);

    // Partial fill: buy 5 at 5000.25
    auto e3 = book->addOrder(makeOrder(3, Side::Buy, 5000.25, 5));
    trackRptSeq(e3);

    auto snap = buildSnapshot(100);

    // Should have 2 ask entries: 5000.25 (5 remaining), 5000.50 (10)
    // No bid entries (buy was fully filled)
    int bid_count = 0, ask_count = 0;
    for (const auto& entry : snap.entries) {
        if (entry.mdEntryType == '0') ++bid_count;
        if (entry.mdEntryType == '1') ++ask_count;
    }
    EXPECT_EQ(bid_count, 0);
    EXPECT_EQ(ask_count, 2);

    // Find the 5000.25 ask entry
    for (const auto& entry : snap.entries) {
        if (entry.mdEntryPx == Price::fromDouble(5000.25).mantissa) {
            EXPECT_EQ(entry.mdEntrySize, 5); // 10 - 5 filled
        }
    }
}

// ---------------------------------------------------------------------------
// Test: Snapshot with empty book has no entries
// ---------------------------------------------------------------------------
TEST_F(SnapshotRecoveryTest, EmptyBookSnapshot) {
    auto snap = buildSnapshot(0);
    EXPECT_EQ(snap.entries.size(), 0u);
    EXPECT_EQ(snap.lastMsgSeqNumProcessed, 0u);
}

// ---------------------------------------------------------------------------
// Test: RptSeq in snapshot matches the last BookUpdate rpt_seq
// ---------------------------------------------------------------------------
TEST_F(SnapshotRecoveryTest, RptSeqMatchesLastUpdate) {
    // Add several orders, tracking rpt_seq
    for (int i = 0; i < 5; ++i) {
        auto events = book->addOrder(makeOrder(i + 1, Side::Buy, 5000.0 - i * 0.25, 10));
        trackRptSeq(events);
    }

    auto snap = buildSnapshot(200);
    EXPECT_EQ(snap.rptSeq, last_rpt_seq_);
    EXPECT_GT(last_rpt_seq_, 0u);
}

// ---------------------------------------------------------------------------
// Test: Snapshot can recover book state via incremental replay
//       (simulate: build from snapshot, verify against book)
// ---------------------------------------------------------------------------
TEST_F(SnapshotRecoveryTest, SnapshotRecoveryConsistency) {
    // Build a book
    auto e1 = book->addOrder(makeOrder(1, Side::Buy, 5000.00, 10));
    trackRptSeq(e1);
    auto e2 = book->addOrder(makeOrder(2, Side::Buy, 5000.00, 5)); // same level
    trackRptSeq(e2);
    auto e3 = book->addOrder(makeOrder(3, Side::Sell, 5000.25, 8));
    trackRptSeq(e3);

    auto snap = buildSnapshot(100);

    // Verify the bid level at 5000.00 has aggregated quantity (10+5=15)
    bool found_bid = false;
    for (const auto& entry : snap.entries) {
        if (entry.mdEntryType == '0' &&
            entry.mdEntryPx == Price::fromDouble(5000.00).mantissa) {
            found_bid = true;
            EXPECT_EQ(entry.mdEntrySize, 15);
            EXPECT_EQ(entry.numberOfOrders, 2);
        }
    }
    EXPECT_TRUE(found_bid);

    // Verify the ask level at 5000.25
    bool found_ask = false;
    for (const auto& entry : snap.entries) {
        if (entry.mdEntryType == '1' &&
            entry.mdEntryPx == Price::fromDouble(5000.25).mantissa) {
            found_ask = true;
            EXPECT_EQ(entry.mdEntrySize, 8);
            EXPECT_EQ(entry.numberOfOrders, 1);
        }
    }
    EXPECT_TRUE(found_ask);
}

// ---------------------------------------------------------------------------
// Test: Snapshot after cancel reflects removed level
// ---------------------------------------------------------------------------
TEST_F(SnapshotRecoveryTest, SnapshotAfterCancel) {
    auto e1 = book->addOrder(makeOrder(1, Side::Buy, 5000.00, 10));
    trackRptSeq(e1);
    auto e2 = book->addOrder(makeOrder(2, Side::Buy, 4999.75, 5));
    trackRptSeq(e2);

    // Cancel order 1
    auto e3 = book->cancelOrder(1);
    trackRptSeq(e3);

    auto snap = buildSnapshot(100);

    // Should only have 1 bid entry at 4999.75
    ASSERT_EQ(snap.entries.size(), 1u);
    EXPECT_EQ(snap.entries[0].mdEntryType, '0');
    EXPECT_EQ(snap.entries[0].mdEntryPx, Price::fromDouble(4999.75).mantissa);
    EXPECT_EQ(snap.entries[0].mdEntrySize, 5);
}
