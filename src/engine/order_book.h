#pragma once
#include "order.h"
#include "trade.h"
#include "price_level.h"
#include "engine_event.h"
#include <map>
#include <unordered_map>
#include <vector>
#include <functional>

namespace cme::sim {

class OrderBook {
public:
    explicit OrderBook(SecurityId security_id);

    // Returns vector of events generated
    std::vector<EngineEvent> addOrder(Order* order);
    std::vector<EngineEvent> cancelOrder(OrderId order_id);
    std::vector<EngineEvent> modifyOrder(OrderId order_id, Price new_price, Quantity new_qty, ClOrdId new_cl_ord_id);

    // Book queries
    Price bestBid() const;
    Price bestAsk() const;
    int bidLevelCount() const;
    int askLevelCount() const;
    const std::map<Price, PriceLevel, std::greater<Price>>& bidLevels() const { return bid_levels_; }
    const std::map<Price, PriceLevel>& askLevels() const { return ask_levels_; }

    SecurityId securityId() const { return security_id_; }

private:
    SecurityId security_id_;
    std::map<Price, PriceLevel, std::greater<Price>> bid_levels_; // descending by price
    std::map<Price, PriceLevel> ask_levels_;                      // ascending by price
    std::unordered_map<OrderId, Order*> orders_by_id_;

    uint64_t next_trade_id_ = 1;
    uint32_t rpt_seq_ = 1;

    std::vector<EngineEvent> matchOrder(Order* order);
    std::vector<EngineEvent> matchLimit(Order* order);
    std::vector<EngineEvent> matchMarket(Order* order);
    bool canFillFOK(Order* order) const;
    void insertResting(Order* order, std::vector<EngineEvent>& events);
    void removeFromBook(Order* order, std::vector<EngineEvent>& events);
    Trade executeTrade(Order* maker, Order* taker, Price trade_price, Quantity trade_qty);

    // Compute 1-based price level index for a given side and price
    int priceLevelIndex(Side side, Price price) const;

    void generateBookUpdate(SecurityId sec_id, Side side, Price price,
                            Quantity new_qty, int new_count,
                            MDUpdateAction action, int level_idx,
                            std::vector<EngineEvent>& events);
};

} // namespace cme::sim
