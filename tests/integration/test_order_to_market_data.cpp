#include <gtest/gtest.h>
#include "engine/order_book.h"
#include "engine/order.h"
#include "engine/engine_event.h"
#include "common/types.h"
#include <memory>
#include <vector>
#include <algorithm>

using namespace cme::sim;

// Integration test: Submit orders through the order book and verify both
// execution report events and market data book update events are generated.

class OrderToMarketDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        book = std::make_unique<OrderBook>(12345); // SecurityId for ES
    }

    Order* makeOrder(OrderId id, Side side, double price, Quantity qty,
                     OrderType type = OrderType::Limit,
                     TimeInForce tif = TimeInForce::Day) {
        auto order = std::make_unique<Order>();
        order->order_id = id;
        order->security_id = 12345;
        order->side = side;
        order->order_type = type;
        order->time_in_force = tif;
        order->price = Price::fromDouble(price);
        order->quantity = qty;
        order->cl_ord_id = "CLO" + std::to_string(id);
        order->session_uuid = 100;
        order->timestamp = id;
        Order* raw = order.get();
        owned_orders_.push_back(std::move(order));
        return raw;
    }

    template <typename T>
    std::vector<T> collectEvents(const std::vector<EngineEvent>& events) {
        std::vector<T> result;
        for (const auto& e : events) {
            if (std::holds_alternative<T>(e)) {
                result.push_back(std::get<T>(e));
            }
        }
        return result;
    }

    std::unique_ptr<OrderBook> book;
    std::vector<std::unique_ptr<Order>> owned_orders_;
};

// ---------------------------------------------------------------------------
// Test: Adding a resting order produces OrderAccepted + BookUpdate(New)
// ---------------------------------------------------------------------------
TEST_F(OrderToMarketDataTest, RestingOrderProducesAcceptAndBookUpdate) {
    Order* buy = makeOrder(1, Side::Buy, 5000.0, 10);
    auto events = book->addOrder(buy);

    auto accepts = collectEvents<OrderAccepted>(events);
    auto updates = collectEvents<BookUpdate>(events);

    ASSERT_EQ(accepts.size(), 1u);
    EXPECT_EQ(accepts[0].order_id, 1u);
    EXPECT_EQ(accepts[0].side, Side::Buy);
    EXPECT_EQ(accepts[0].price, Price::fromDouble(5000.0));
    EXPECT_EQ(accepts[0].quantity, 10);

    ASSERT_EQ(updates.size(), 1u);
    EXPECT_EQ(updates[0].security_id, 12345);
    EXPECT_EQ(updates[0].side, Side::Buy);
    EXPECT_EQ(updates[0].price, Price::fromDouble(5000.0));
    EXPECT_EQ(updates[0].new_qty, 10);
    EXPECT_EQ(updates[0].new_order_count, 1);
    EXPECT_EQ(updates[0].update_action, MDUpdateAction::New);
    EXPECT_EQ(updates[0].price_level_index, 1);
}

// ---------------------------------------------------------------------------
// Test: Matching order produces OrderAccepted + OrderFilled + BookUpdate(Delete)
// ---------------------------------------------------------------------------
TEST_F(OrderToMarketDataTest, MatchProducesAcceptFillAndBookDelete) {
    // Step 1: Add sell order that rests
    Order* sell = makeOrder(1, Side::Sell, 5000.25, 10);
    book->addOrder(sell);

    // Step 2: Add crossing buy order
    Order* buy = makeOrder(2, Side::Buy, 5000.25, 10);
    auto events = book->addOrder(buy);

    auto accepts = collectEvents<OrderAccepted>(events);
    auto fills = collectEvents<OrderFilled>(events);
    auto updates = collectEvents<BookUpdate>(events);

    // Should have accepted the buy
    ASSERT_EQ(accepts.size(), 1u);
    EXPECT_EQ(accepts[0].order_id, 2u);

    // Should have one fill
    ASSERT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].trade_qty, 10);
    EXPECT_EQ(fills[0].trade_price, Price::fromDouble(5000.25));
    EXPECT_EQ(fills[0].aggressor_side, Side::Buy);
    EXPECT_EQ(fills[0].maker_order_id, 1u);
    EXPECT_EQ(fills[0].taker_order_id, 2u);
    EXPECT_EQ(fills[0].maker_ord_status, OrdStatus::Filled);
    EXPECT_EQ(fills[0].taker_ord_status, OrdStatus::Filled);

    // BookUpdate should show Delete for the ask level
    ASSERT_GE(updates.size(), 1u);
    bool found_delete = false;
    for (const auto& upd : updates) {
        if (upd.update_action == MDUpdateAction::Delete && upd.side == Side::Sell) {
            found_delete = true;
            EXPECT_EQ(upd.price, Price::fromDouble(5000.25));
            EXPECT_EQ(upd.new_qty, 0);
            EXPECT_EQ(upd.new_order_count, 0);
        }
    }
    EXPECT_TRUE(found_delete);
}

// ---------------------------------------------------------------------------
// Test: Partial fill produces Change update, not Delete
// ---------------------------------------------------------------------------
TEST_F(OrderToMarketDataTest, PartialFillProducesBookChange) {
    Order* sell1 = makeOrder(1, Side::Sell, 5000.25, 10);
    Order* sell2 = makeOrder(2, Side::Sell, 5000.25, 5);
    book->addOrder(sell1);
    book->addOrder(sell2);

    // Buy 10 out of 15 -> partial fill, 5 remaining
    Order* buy = makeOrder(3, Side::Buy, 5000.25, 10);
    auto events = book->addOrder(buy);

    auto updates = collectEvents<BookUpdate>(events);

    // After consuming sell1 (10), the level still has sell2 (5)
    // We expect: Delete (when sell1 fully removed) or Change if sell2 remains
    // The matching fills sell1 fully (Delete level then re-add? No - it processes within the level)
    // Actually: sell1 fully filled -> removed from level, but sell2 remains -> level goes from
    // 15 qty to 5 qty = Change update
    bool found_change = false;
    for (const auto& upd : updates) {
        if (upd.update_action == MDUpdateAction::Change && upd.side == Side::Sell) {
            found_change = true;
            EXPECT_EQ(upd.new_qty, 5);
            EXPECT_EQ(upd.new_order_count, 1);
        }
    }
    EXPECT_TRUE(found_change);
}

// ---------------------------------------------------------------------------
// Test: Multi-level sweep produces multiple BookUpdate events
// ---------------------------------------------------------------------------
TEST_F(OrderToMarketDataTest, MultiLevelSweepMultipleUpdates) {
    Order* s1 = makeOrder(1, Side::Sell, 5000.25, 5);
    Order* s2 = makeOrder(2, Side::Sell, 5000.50, 5);
    Order* s3 = makeOrder(3, Side::Sell, 5000.75, 5);
    book->addOrder(s1);
    book->addOrder(s2);
    book->addOrder(s3);

    // Buy sweeps all three levels
    Order* buy = makeOrder(4, Side::Buy, 5000.75, 15);
    auto events = book->addOrder(buy);

    auto fills = collectEvents<OrderFilled>(events);
    auto updates = collectEvents<BookUpdate>(events);

    // 3 fills, one per level
    EXPECT_EQ(fills.size(), 3u);
    // 3 book updates (Delete for each emptied level)
    EXPECT_EQ(updates.size(), 3u);

    for (const auto& upd : updates) {
        EXPECT_EQ(upd.update_action, MDUpdateAction::Delete);
        EXPECT_EQ(upd.side, Side::Sell);
    }
}

// ---------------------------------------------------------------------------
// Test: Cancel generates BookUpdate(Delete) and OrderCancelled
// ---------------------------------------------------------------------------
TEST_F(OrderToMarketDataTest, CancelGeneratesBookDeleteAndCancelled) {
    Order* buy = makeOrder(1, Side::Buy, 5000.0, 10);
    book->addOrder(buy);

    auto events = book->cancelOrder(1);

    auto cancels = collectEvents<OrderCancelled>(events);
    auto updates = collectEvents<BookUpdate>(events);

    ASSERT_EQ(cancels.size(), 1u);
    EXPECT_EQ(cancels[0].order_id, 1u);
    EXPECT_EQ(cancels[0].ord_status, OrdStatus::Canceled);

    ASSERT_EQ(updates.size(), 1u);
    EXPECT_EQ(updates[0].update_action, MDUpdateAction::Delete);
    EXPECT_EQ(updates[0].side, Side::Buy);
    EXPECT_EQ(updates[0].price, Price::fromDouble(5000.0));
}

// ---------------------------------------------------------------------------
// Test: Modify produces BookUpdate(Delete old) + BookUpdate(New at new price)
// ---------------------------------------------------------------------------
TEST_F(OrderToMarketDataTest, ModifyGeneratesBookUpdates) {
    Order* buy = makeOrder(1, Side::Buy, 5000.0, 10);
    book->addOrder(buy);

    auto events = book->modifyOrder(1, Price::fromDouble(5001.0), 10, "NEWCL1");

    auto modifies = collectEvents<OrderModified>(events);
    auto updates = collectEvents<BookUpdate>(events);

    ASSERT_EQ(modifies.size(), 1u);
    EXPECT_EQ(modifies[0].new_price, Price::fromDouble(5001.0));

    // Should have Delete(5000) and New(5001)
    bool found_delete = false;
    bool found_new = false;
    for (const auto& upd : updates) {
        if (upd.update_action == MDUpdateAction::Delete && upd.price == Price::fromDouble(5000.0)) {
            found_delete = true;
        }
        if (upd.update_action == MDUpdateAction::New && upd.price == Price::fromDouble(5001.0)) {
            found_new = true;
        }
    }
    EXPECT_TRUE(found_delete);
    EXPECT_TRUE(found_new);
}

// ---------------------------------------------------------------------------
// Test: RptSeq increases monotonically across book updates
// ---------------------------------------------------------------------------
TEST_F(OrderToMarketDataTest, RptSeqMonotonicallyIncreases) {
    Order* b1 = makeOrder(1, Side::Buy, 5000.0, 10);
    Order* b2 = makeOrder(2, Side::Buy, 5001.0, 5);
    Order* b3 = makeOrder(3, Side::Buy, 5002.0, 3);

    std::vector<BookUpdate> all_updates;

    auto e1 = book->addOrder(b1);
    for (const auto& ev : e1) {
        if (std::holds_alternative<BookUpdate>(ev))
            all_updates.push_back(std::get<BookUpdate>(ev));
    }

    auto e2 = book->addOrder(b2);
    for (const auto& ev : e2) {
        if (std::holds_alternative<BookUpdate>(ev))
            all_updates.push_back(std::get<BookUpdate>(ev));
    }

    auto e3 = book->addOrder(b3);
    for (const auto& ev : e3) {
        if (std::holds_alternative<BookUpdate>(ev))
            all_updates.push_back(std::get<BookUpdate>(ev));
    }

    ASSERT_GE(all_updates.size(), 3u);
    for (size_t i = 1; i < all_updates.size(); ++i) {
        EXPECT_GT(all_updates[i].rpt_seq, all_updates[i - 1].rpt_seq);
    }
}

// ---------------------------------------------------------------------------
// Test: Full scenario - build book, cross, verify state
// ---------------------------------------------------------------------------
TEST_F(OrderToMarketDataTest, FullScenario) {
    // Build a book: 3 bid levels, 3 ask levels
    Order* b1 = makeOrder(1, Side::Buy, 5000.00, 10);
    Order* b2 = makeOrder(2, Side::Buy, 4999.75, 10);
    Order* b3 = makeOrder(3, Side::Buy, 4999.50, 10);
    Order* s1 = makeOrder(4, Side::Sell, 5000.25, 10);
    Order* s2 = makeOrder(5, Side::Sell, 5000.50, 10);
    Order* s3 = makeOrder(6, Side::Sell, 5000.75, 10);

    book->addOrder(b1);
    book->addOrder(b2);
    book->addOrder(b3);
    book->addOrder(s1);
    book->addOrder(s2);
    book->addOrder(s3);

    EXPECT_EQ(book->bidLevelCount(), 3);
    EXPECT_EQ(book->askLevelCount(), 3);
    EXPECT_EQ(book->bestBid(), Price::fromDouble(5000.00));
    EXPECT_EQ(book->bestAsk(), Price::fromDouble(5000.25));

    // Aggressive sell crosses the top bid
    Order* sell_agg = makeOrder(7, Side::Sell, 5000.00, 5);
    auto events = book->addOrder(sell_agg);

    auto fills = collectEvents<OrderFilled>(events);
    ASSERT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].trade_qty, 5);
    EXPECT_EQ(fills[0].trade_price, Price::fromDouble(5000.00));

    // Best bid still 5000 but with reduced qty
    EXPECT_EQ(book->bestBid(), Price::fromDouble(5000.00));
    EXPECT_EQ(book->bidLevelCount(), 3);

    // Sell sweeps remaining bid at 5000
    Order* sell_agg2 = makeOrder(8, Side::Sell, 5000.00, 5);
    auto events2 = book->addOrder(sell_agg2);

    auto fills2 = collectEvents<OrderFilled>(events2);
    ASSERT_EQ(fills2.size(), 1u);

    // Now best bid should be 4999.75
    EXPECT_EQ(book->bestBid(), Price::fromDouble(4999.75));
    EXPECT_EQ(book->bidLevelCount(), 2);
}
