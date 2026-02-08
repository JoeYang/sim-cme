#include <gtest/gtest.h>
#include "engine/order_book.h"
#include "engine/order.h"
#include "common/types.h"
#include <memory>
#include <vector>
#include <algorithm>

using namespace cme::sim;

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        book = std::make_unique<OrderBook>(1);
    }

    // Allocate Order on the heap and track ownership.
    // The book stores raw pointers, so we keep the Orders alive here.
    Order* makeOrder(OrderId id, Side side, double price, Quantity qty,
                     OrderType type = OrderType::Limit,
                     TimeInForce tif = TimeInForce::Day) {
        auto order = std::make_unique<Order>();
        order->order_id = id;
        order->security_id = 1;
        order->side = side;
        order->order_type = type;
        order->time_in_force = tif;
        order->price = Price::fromDouble(price);
        order->quantity = qty;
        order->cl_ord_id = "CLO" + std::to_string(id);
        order->session_uuid = 100;
        order->timestamp = id; // use id as timestamp for ordering
        Order* raw = order.get();
        owned_orders_.push_back(std::move(order));
        return raw;
    }

    // Helpers to count specific event types in a vector
    template <typename T>
    int countEvents(const std::vector<EngineEvent>& events) {
        int n = 0;
        for (const auto& e : events) {
            if (std::holds_alternative<T>(e)) ++n;
        }
        return n;
    }

    template <typename T>
    const T& getEvent(const std::vector<EngineEvent>& events, int index = 0) {
        int n = 0;
        for (const auto& e : events) {
            if (std::holds_alternative<T>(e)) {
                if (n == index) return std::get<T>(e);
                ++n;
            }
        }
        throw std::runtime_error("Event not found");
    }

    std::unique_ptr<OrderBook> book;
    std::vector<std::unique_ptr<Order>> owned_orders_;
};

// ---------------------------------------------------------------------------
// 1. AddSingleBuyOrder
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, AddSingleBuyOrder) {
    Order* o = makeOrder(1, Side::Buy, 100.0, 10);
    auto events = book->addOrder(o);

    ASSERT_EQ(countEvents<OrderAccepted>(events), 1);
    EXPECT_EQ(book->bidLevelCount(), 1);
    EXPECT_EQ(book->askLevelCount(), 0);
    EXPECT_EQ(book->bestBid(), Price::fromDouble(100.0));
    EXPECT_TRUE(book->bestAsk().isNull());
}

// ---------------------------------------------------------------------------
// 2. AddSingleSellOrder
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, AddSingleSellOrder) {
    Order* o = makeOrder(1, Side::Sell, 101.0, 5);
    auto events = book->addOrder(o);

    ASSERT_EQ(countEvents<OrderAccepted>(events), 1);
    EXPECT_EQ(book->askLevelCount(), 1);
    EXPECT_EQ(book->bidLevelCount(), 0);
    EXPECT_EQ(book->bestAsk(), Price::fromDouble(101.0));
    EXPECT_TRUE(book->bestBid().isNull());
}

// ---------------------------------------------------------------------------
// 3. MatchBuyCrossesAsk
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, MatchBuyCrossesAsk) {
    Order* sell = makeOrder(1, Side::Sell, 100.0, 10);
    book->addOrder(sell);

    Order* buy = makeOrder(2, Side::Buy, 100.0, 10);
    auto events = book->addOrder(buy);

    EXPECT_GE(countEvents<OrderFilled>(events), 1);
    auto& fill = getEvent<OrderFilled>(events);
    EXPECT_EQ(fill.trade_qty, 10);
    EXPECT_EQ(fill.trade_price, Price::fromDouble(100.0));
    EXPECT_EQ(fill.aggressor_side, Side::Buy);

    // Book should be empty after full match
    EXPECT_EQ(book->bidLevelCount(), 0);
    EXPECT_EQ(book->askLevelCount(), 0);
}

// ---------------------------------------------------------------------------
// 4. MatchSellCrossesBid
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, MatchSellCrossesBid) {
    Order* buy = makeOrder(1, Side::Buy, 100.0, 10);
    book->addOrder(buy);

    Order* sell = makeOrder(2, Side::Sell, 100.0, 10);
    auto events = book->addOrder(sell);

    EXPECT_GE(countEvents<OrderFilled>(events), 1);
    auto& fill = getEvent<OrderFilled>(events);
    EXPECT_EQ(fill.trade_qty, 10);
    EXPECT_EQ(fill.aggressor_side, Side::Sell);

    EXPECT_EQ(book->bidLevelCount(), 0);
    EXPECT_EQ(book->askLevelCount(), 0);
}

// ---------------------------------------------------------------------------
// 5. PartialFill
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, PartialFill) {
    Order* buy = makeOrder(1, Side::Buy, 100.0, 10);
    book->addOrder(buy);

    Order* sell = makeOrder(2, Side::Sell, 100.0, 5);
    auto events = book->addOrder(sell);

    ASSERT_GE(countEvents<OrderFilled>(events), 1);
    auto& fill = getEvent<OrderFilled>(events);
    EXPECT_EQ(fill.trade_qty, 5);

    // Maker (buy) should still be resting with 5 remaining
    EXPECT_EQ(buy->remainingQty(), 5);
    EXPECT_EQ(buy->filled_qty, 5);
    EXPECT_EQ(book->bidLevelCount(), 1);
    EXPECT_EQ(book->askLevelCount(), 0);
}

// ---------------------------------------------------------------------------
// 6. MultiplePriceLevels
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, MultiplePriceLevels) {
    Order* b1 = makeOrder(1, Side::Buy, 99.0, 10);
    Order* b2 = makeOrder(2, Side::Buy, 100.0, 10);
    Order* b3 = makeOrder(3, Side::Buy, 98.0, 10);
    book->addOrder(b1);
    book->addOrder(b2);
    book->addOrder(b3);

    EXPECT_EQ(book->bidLevelCount(), 3);
    // Best bid should be 100 (highest)
    EXPECT_EQ(book->bestBid(), Price::fromDouble(100.0));

    Order* s1 = makeOrder(4, Side::Sell, 101.0, 10);
    Order* s2 = makeOrder(5, Side::Sell, 102.0, 10);
    book->addOrder(s1);
    book->addOrder(s2);

    EXPECT_EQ(book->askLevelCount(), 2);
    // Best ask should be 101 (lowest)
    EXPECT_EQ(book->bestAsk(), Price::fromDouble(101.0));
}

// ---------------------------------------------------------------------------
// 7. TimeInForceFIFO
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, TimeInForceFIFO) {
    // Two buys at same price; first one should fill first
    Order* b1 = makeOrder(1, Side::Buy, 100.0, 5);
    Order* b2 = makeOrder(2, Side::Buy, 100.0, 5);
    book->addOrder(b1);
    book->addOrder(b2);

    // Sell 5 -> should match b1 (FIFO)
    Order* sell = makeOrder(3, Side::Sell, 100.0, 5);
    auto events = book->addOrder(sell);

    auto& fill = getEvent<OrderFilled>(events);
    EXPECT_EQ(fill.maker_order_id, 1u); // b1 was first
    EXPECT_EQ(fill.trade_qty, 5);

    // b1 fully filled, b2 still resting
    EXPECT_TRUE(b1->isFullyFilled());
    EXPECT_EQ(b2->filled_qty, 0);
    EXPECT_EQ(book->bidLevelCount(), 1);
}

// ---------------------------------------------------------------------------
// 8. CancelOrder
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, CancelOrder) {
    Order* buy = makeOrder(1, Side::Buy, 100.0, 10);
    book->addOrder(buy);
    EXPECT_EQ(book->bidLevelCount(), 1);

    auto events = book->cancelOrder(1);
    ASSERT_EQ(countEvents<OrderCancelled>(events), 1);
    EXPECT_EQ(book->bidLevelCount(), 0);
}

// ---------------------------------------------------------------------------
// 9. CancelNonexistentOrder
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, CancelNonexistentOrder) {
    auto events = book->cancelOrder(999);
    ASSERT_EQ(countEvents<OrderCancelRejected>(events), 1);
}

// ---------------------------------------------------------------------------
// 10. ModifyOrderPrice
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, ModifyOrderPrice) {
    Order* buy = makeOrder(1, Side::Buy, 99.0, 10);
    book->addOrder(buy);
    EXPECT_EQ(book->bestBid(), Price::fromDouble(99.0));

    auto events = book->modifyOrder(1, Price::fromDouble(101.0), 10, "NEWCLO1");
    ASSERT_GE(countEvents<OrderModified>(events), 1);

    auto& mod = getEvent<OrderModified>(events);
    EXPECT_EQ(mod.new_price, Price::fromDouble(101.0));

    EXPECT_EQ(book->bestBid(), Price::fromDouble(101.0));
    EXPECT_EQ(book->bidLevelCount(), 1);
}

// ---------------------------------------------------------------------------
// 11. ModifyQuantity
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, ModifyQuantity) {
    Order* buy = makeOrder(1, Side::Buy, 100.0, 10);
    book->addOrder(buy);

    auto events = book->modifyOrder(1, Price::fromDouble(100.0), 5, "NEWCLO1");
    ASSERT_GE(countEvents<OrderModified>(events), 1);

    // The level should now show 5 total quantity
    auto& levels = book->bidLevels();
    auto it = levels.begin();
    ASSERT_NE(it, levels.end());
    EXPECT_EQ(it->second.total_quantity, 5);
}

// ---------------------------------------------------------------------------
// 12. IOCOrderFilled
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, IOCOrderFilled) {
    Order* sell = makeOrder(1, Side::Sell, 100.0, 10);
    book->addOrder(sell);

    Order* ioc_buy = makeOrder(2, Side::Buy, 100.0, 10, OrderType::Limit, TimeInForce::IOC);
    auto events = book->addOrder(ioc_buy);

    EXPECT_GE(countEvents<OrderFilled>(events), 1);
    // IOC fully filled, should not rest
    EXPECT_EQ(book->bidLevelCount(), 0);
    EXPECT_EQ(book->askLevelCount(), 0);
}

// ---------------------------------------------------------------------------
// 13. IOCOrderPartialElimination
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, IOCOrderPartialElimination) {
    Order* sell = makeOrder(1, Side::Sell, 100.0, 5);
    book->addOrder(sell);

    Order* ioc_buy = makeOrder(2, Side::Buy, 100.0, 10, OrderType::Limit, TimeInForce::IOC);
    auto events = book->addOrder(ioc_buy);

    EXPECT_GE(countEvents<OrderFilled>(events), 1);
    // Remaining 5 should be cancelled (IOC)
    EXPECT_GE(countEvents<OrderCancelled>(events), 1);
    EXPECT_EQ(ioc_buy->filled_qty, 5);
    EXPECT_EQ(book->bidLevelCount(), 0);
}

// ---------------------------------------------------------------------------
// 14. IOCOrderNoMatch
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, IOCOrderNoMatch) {
    // Empty book, IOC buy should be immediately cancelled
    Order* ioc_buy = makeOrder(1, Side::Buy, 100.0, 10, OrderType::Limit, TimeInForce::IOC);
    auto events = book->addOrder(ioc_buy);

    EXPECT_EQ(countEvents<OrderFilled>(events), 0);
    EXPECT_GE(countEvents<OrderCancelled>(events), 1);
    EXPECT_EQ(book->bidLevelCount(), 0);
}

// ---------------------------------------------------------------------------
// 15. FOKOrderFilled
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, FOKOrderFilled) {
    Order* s1 = makeOrder(1, Side::Sell, 100.0, 5);
    Order* s2 = makeOrder(2, Side::Sell, 100.0, 5);
    book->addOrder(s1);
    book->addOrder(s2);

    Order* fok_buy = makeOrder(3, Side::Buy, 100.0, 10, OrderType::Limit, TimeInForce::FOK);
    auto events = book->addOrder(fok_buy);

    // FOK should fill completely since 5+5 = 10 available
    EXPECT_GE(countEvents<OrderFilled>(events), 1);
    EXPECT_TRUE(fok_buy->isFullyFilled());
    EXPECT_EQ(book->askLevelCount(), 0);
}

// ---------------------------------------------------------------------------
// 16. FOKOrderRejected
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, FOKOrderRejected) {
    Order* sell = makeOrder(1, Side::Sell, 100.0, 5);
    book->addOrder(sell);

    // FOK buy for 10 but only 5 available -> rejected
    Order* fok_buy = makeOrder(2, Side::Buy, 100.0, 10, OrderType::Limit, TimeInForce::FOK);
    auto events = book->addOrder(fok_buy);

    EXPECT_GE(countEvents<OrderRejected>(events), 1);
    EXPECT_EQ(countEvents<OrderFilled>(events), 0);
    // Sell should still be resting
    EXPECT_EQ(book->askLevelCount(), 1);
    EXPECT_EQ(sell->filled_qty, 0);
}

// ---------------------------------------------------------------------------
// 17. MarketOrderMatch
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, MarketOrderMatch) {
    Order* s1 = makeOrder(1, Side::Sell, 100.0, 5);
    Order* s2 = makeOrder(2, Side::Sell, 101.0, 5);
    book->addOrder(s1);
    book->addOrder(s2);

    Order* mkt_buy = makeOrder(3, Side::Buy, 0.0, 10, OrderType::Market);
    auto events = book->addOrder(mkt_buy);

    // Should sweep both ask levels
    EXPECT_GE(countEvents<OrderFilled>(events), 2);
    EXPECT_TRUE(mkt_buy->isFullyFilled());
    EXPECT_EQ(book->askLevelCount(), 0);
}

// ---------------------------------------------------------------------------
// 18. EmptyBookBehavior
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, EmptyBookBehavior) {
    EXPECT_TRUE(book->bestBid().isNull());
    EXPECT_TRUE(book->bestAsk().isNull());
    EXPECT_EQ(book->bidLevelCount(), 0);
    EXPECT_EQ(book->askLevelCount(), 0);

    // Cancel on empty book
    auto events = book->cancelOrder(999);
    EXPECT_EQ(countEvents<OrderCancelRejected>(events), 1);
}

// ---------------------------------------------------------------------------
// 19. MultiLevelMatching
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, MultiLevelMatching) {
    // Set up three ask levels
    Order* s1 = makeOrder(1, Side::Sell, 100.0, 3);
    Order* s2 = makeOrder(2, Side::Sell, 101.0, 4);
    Order* s3 = makeOrder(3, Side::Sell, 102.0, 5);
    book->addOrder(s1);
    book->addOrder(s2);
    book->addOrder(s3);

    // Buy crosses all three levels
    Order* buy = makeOrder(4, Side::Buy, 102.0, 12);
    auto events = book->addOrder(buy);

    int fills = countEvents<OrderFilled>(events);
    EXPECT_GE(fills, 3); // at least one fill per level

    // Entire 12 should be filled (3+4+5)
    EXPECT_TRUE(buy->isFullyFilled());
    EXPECT_EQ(book->askLevelCount(), 0);
}

// ---------------------------------------------------------------------------
// 20. BookUpdateEvents
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, BookUpdateEvents) {
    // Adding an order should produce a BookUpdate with New action
    Order* buy = makeOrder(1, Side::Buy, 100.0, 10);
    auto events = book->addOrder(buy);

    ASSERT_GE(countEvents<BookUpdate>(events), 1);
    auto& bu = getEvent<BookUpdate>(events);
    EXPECT_EQ(bu.update_action, MDUpdateAction::New);
    EXPECT_EQ(bu.side, Side::Buy);
    EXPECT_EQ(bu.price, Price::fromDouble(100.0));
    EXPECT_EQ(bu.new_qty, 10);
    EXPECT_EQ(bu.new_order_count, 1);
    EXPECT_EQ(bu.price_level_index, 1);

    // Cancel should produce BookUpdate with Delete action
    auto cancel_events = book->cancelOrder(1);
    ASSERT_GE(countEvents<BookUpdate>(cancel_events), 1);
    auto& del = getEvent<BookUpdate>(cancel_events);
    EXPECT_EQ(del.update_action, MDUpdateAction::Delete);

    // Adding two orders at same price: second should produce Change
    Order* b1 = makeOrder(10, Side::Buy, 100.0, 5);
    Order* b2 = makeOrder(11, Side::Buy, 100.0, 3);
    book->addOrder(b1);
    auto add_events = book->addOrder(b2);

    // The book update from adding b2 should be Change (level already exists)
    bool found_change = false;
    for (const auto& ev : add_events) {
        if (std::holds_alternative<BookUpdate>(ev)) {
            auto& upd = std::get<BookUpdate>(ev);
            if (upd.update_action == MDUpdateAction::Change) {
                found_change = true;
                EXPECT_EQ(upd.new_qty, 8); // 5+3
                EXPECT_EQ(upd.new_order_count, 2);
            }
        }
    }
    EXPECT_TRUE(found_change);
}

// ---------------------------------------------------------------------------
// 21. LargeScale
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, LargeScale) {
    // Add 500 buy orders at different prices
    for (int i = 0; i < 500; ++i) {
        Order* o = makeOrder(i + 1, Side::Buy, 90.0 + i * 0.01, 1);
        book->addOrder(o);
    }
    EXPECT_EQ(book->bidLevelCount(), 500);

    // Add 500 sell orders at different prices
    for (int i = 0; i < 500; ++i) {
        Order* o = makeOrder(1000 + i, Side::Sell, 200.0 + i * 0.01, 1);
        book->addOrder(o);
    }
    EXPECT_EQ(book->askLevelCount(), 500);

    // Best bid should be the highest buy
    EXPECT_EQ(book->bestBid(), Price::fromDouble(90.0 + 499 * 0.01));
    // Best ask should be the lowest sell
    EXPECT_EQ(book->bestAsk(), Price::fromDouble(200.0));
}

// ---------------------------------------------------------------------------
// 22. MarketOrderEmptyBook
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, MarketOrderEmptyBook) {
    Order* mkt = makeOrder(1, Side::Buy, 0.0, 10, OrderType::Market);
    auto events = book->addOrder(mkt);

    // Market order on empty book: accepted then cancelled
    EXPECT_EQ(countEvents<OrderFilled>(events), 0);
    EXPECT_GE(countEvents<OrderCancelled>(events), 1);
}

// ---------------------------------------------------------------------------
// 23. ModifyNonexistentOrder
// ---------------------------------------------------------------------------
TEST_F(OrderBookTest, ModifyNonexistentOrder) {
    auto events = book->modifyOrder(999, Price::fromDouble(100.0), 10, "CL999");
    EXPECT_GE(countEvents<OrderCancelRejected>(events), 1);
}
