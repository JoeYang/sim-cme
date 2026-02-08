#include "order_book.h"
#include <algorithm>
#include <cassert>

namespace cme::sim {

OrderBook::OrderBook(SecurityId security_id)
    : security_id_(security_id) {}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

std::vector<EngineEvent> OrderBook::addOrder(Order* order) {
    std::vector<EngineEvent> events;

    // FOK: check total available quantity before doing anything
    if (order->time_in_force == TimeInForce::FOK) {
        if (!canFillFOK(order)) {
            events.emplace_back(OrderRejected{
                order->cl_ord_id,
                order->session_uuid,
                "FOK order cannot be fully filled",
                0
            });
            return events;
        }
    }

    // Accept the order
    events.emplace_back(OrderAccepted{
        order->order_id,
        order->cl_ord_id,
        order->session_uuid,
        order->security_id,
        order->side,
        order->price,
        order->quantity,
        order->order_type,
        order->time_in_force
    });

    // Attempt matching
    auto match_events = matchOrder(order);
    events.insert(events.end(), match_events.begin(), match_events.end());

    // Post-match handling based on TIF
    if (!order->isFullyFilled()) {
        if (order->time_in_force == TimeInForce::IOC ||
            order->time_in_force == TimeInForce::FOK) {
            // Cancel remaining quantity
            order->status = OrdStatus::Canceled;
            events.emplace_back(OrderCancelled{
                order->order_id,
                order->cl_ord_id,
                order->session_uuid,
                order->security_id,
                order->filled_qty,
                order->status
            });
        } else if (order->order_type == OrderType::Limit) {
            // Day/GTC: insert as resting order
            insertResting(order, events);
        } else if (order->order_type == OrderType::Market) {
            // Market order with no liquidity remaining: cancel rest
            order->status = OrdStatus::Canceled;
            events.emplace_back(OrderCancelled{
                order->order_id,
                order->cl_ord_id,
                order->session_uuid,
                order->security_id,
                order->filled_qty,
                order->status
            });
        }
    }

    return events;
}

std::vector<EngineEvent> OrderBook::cancelOrder(OrderId order_id) {
    std::vector<EngineEvent> events;

    auto it = orders_by_id_.find(order_id);
    if (it == orders_by_id_.end()) {
        events.emplace_back(OrderCancelRejected{
            order_id,
            ClOrdId{},
            0,
            0,
            "Order not found"
        });
        return events;
    }

    Order* order = it->second;
    removeFromBook(order, events);

    order->status = OrdStatus::Canceled;
    events.emplace_back(OrderCancelled{
        order->order_id,
        order->cl_ord_id,
        order->session_uuid,
        order->security_id,
        order->filled_qty,
        order->status
    });

    return events;
}

std::vector<EngineEvent> OrderBook::modifyOrder(OrderId order_id, Price new_price, Quantity new_qty, ClOrdId new_cl_ord_id) {
    std::vector<EngineEvent> events;

    auto it = orders_by_id_.find(order_id);
    if (it == orders_by_id_.end()) {
        events.emplace_back(OrderCancelRejected{
            order_id,
            new_cl_ord_id,
            0,
            0,
            "Order not found"
        });
        return events;
    }

    Order* order = it->second;

    // Remove from current position
    removeFromBook(order, events);

    // Update fields
    Price old_price = order->price;
    order->price = new_price;
    order->quantity = new_qty;
    if (!new_cl_ord_id.empty()) {
        order->cl_ord_id = new_cl_ord_id;
    }
    order->status = OrdStatus::Replaced;

    // Generate modify event
    events.emplace_back(OrderModified{
        order->order_id,
        order->cl_ord_id,
        order->session_uuid,
        order->security_id,
        new_price,
        new_qty,
        order->filled_qty,
        order->remainingQty()
    });

    // Re-match at new price
    auto match_events = matchOrder(order);
    events.insert(events.end(), match_events.begin(), match_events.end());

    // If not fully filled, re-insert as resting
    if (!order->isFullyFilled() && order->order_type == OrderType::Limit) {
        insertResting(order, events);
    }

    return events;
}

// ---------------------------------------------------------------------------
// Book queries
// ---------------------------------------------------------------------------

Price OrderBook::bestBid() const {
    if (bid_levels_.empty()) return Price::null();
    return bid_levels_.begin()->first;
}

Price OrderBook::bestAsk() const {
    if (ask_levels_.empty()) return Price::null();
    return ask_levels_.begin()->first;
}

int OrderBook::bidLevelCount() const {
    return static_cast<int>(bid_levels_.size());
}

int OrderBook::askLevelCount() const {
    return static_cast<int>(ask_levels_.size());
}

// ---------------------------------------------------------------------------
// Matching logic
// ---------------------------------------------------------------------------

std::vector<EngineEvent> OrderBook::matchOrder(Order* order) {
    if (order->order_type == OrderType::Market) {
        return matchMarket(order);
    }
    return matchLimit(order);
}

std::vector<EngineEvent> OrderBook::matchLimit(Order* order) {
    std::vector<EngineEvent> events;

    if (order->side == Side::Buy) {
        // Buy: match against asks where ask_price <= order_price
        // ask_levels_ is ascending, so iterate from begin (lowest ask)
        auto it = ask_levels_.begin();
        while (it != ask_levels_.end() && order->remainingQty() > 0) {
            if (it->first > order->price) break; // ask too expensive

            PriceLevel& level = it->second;
            Price trade_price = it->first;

            while (!level.empty() && order->remainingQty() > 0) {
                Order* maker = level.front();
                Quantity trade_qty = std::min(order->remainingQty(), maker->remainingQty());

                Quantity old_level_qty = level.total_quantity;
                int old_level_count = level.order_count;

                Trade trade = executeTrade(maker, order, trade_price, trade_qty);

                // Adjust level quantity before removeOrder (which uses
                // remainingQty(), already 0 for a fully-filled maker).
                level.total_quantity -= trade_qty;

                // Update maker status
                if (maker->isFullyFilled()) {
                    maker->status = OrdStatus::Filled;
                    level.removeOrder(maker);
                    orders_by_id_.erase(maker->order_id);
                } else {
                    maker->status = OrdStatus::PartiallyFilled;
                }

                // Update taker status
                if (order->isFullyFilled()) {
                    order->status = OrdStatus::Filled;
                } else {
                    order->status = OrdStatus::PartiallyFilled;
                }

                // Generate fill event
                events.emplace_back(OrderFilled{
                    trade.trade_id,
                    trade.security_id,
                    trade.price,
                    trade.quantity,
                    trade.aggressor_side,
                    maker->order_id, maker->cl_ord_id, maker->session_uuid,
                    maker->filled_qty, maker->remainingQty(), maker->status,
                    order->order_id, order->cl_ord_id, order->session_uuid,
                    order->filled_qty, order->remainingQty(), order->status
                });

                // Book update for the ask level change
                int level_idx = priceLevelIndex(Side::Sell, trade_price);
                if (level.empty()) {
                    generateBookUpdate(security_id_, Side::Sell, trade_price,
                                       0, 0, MDUpdateAction::Delete, level_idx, events);
                } else {
                    generateBookUpdate(security_id_, Side::Sell, trade_price,
                                       level.total_quantity, level.order_count,
                                       MDUpdateAction::Change, level_idx, events);
                }
            }

            if (level.empty()) {
                it = ask_levels_.erase(it);
            } else {
                ++it;
            }
        }
    } else {
        // Sell: match against bids where bid_price >= order_price
        // bid_levels_ is descending, so iterate from begin (highest bid)
        auto it = bid_levels_.begin();
        while (it != bid_levels_.end() && order->remainingQty() > 0) {
            if (it->first < order->price) break; // bid too low

            PriceLevel& level = it->second;
            Price trade_price = it->first;

            while (!level.empty() && order->remainingQty() > 0) {
                Order* maker = level.front();
                Quantity trade_qty = std::min(order->remainingQty(), maker->remainingQty());

                Trade trade = executeTrade(maker, order, trade_price, trade_qty);

                level.total_quantity -= trade_qty;

                if (maker->isFullyFilled()) {
                    maker->status = OrdStatus::Filled;
                    level.removeOrder(maker);
                    orders_by_id_.erase(maker->order_id);
                } else {
                    maker->status = OrdStatus::PartiallyFilled;
                }

                if (order->isFullyFilled()) {
                    order->status = OrdStatus::Filled;
                } else {
                    order->status = OrdStatus::PartiallyFilled;
                }

                events.emplace_back(OrderFilled{
                    trade.trade_id,
                    trade.security_id,
                    trade.price,
                    trade.quantity,
                    trade.aggressor_side,
                    maker->order_id, maker->cl_ord_id, maker->session_uuid,
                    maker->filled_qty, maker->remainingQty(), maker->status,
                    order->order_id, order->cl_ord_id, order->session_uuid,
                    order->filled_qty, order->remainingQty(), order->status
                });

                int level_idx = priceLevelIndex(Side::Buy, trade_price);
                if (level.empty()) {
                    generateBookUpdate(security_id_, Side::Buy, trade_price,
                                       0, 0, MDUpdateAction::Delete, level_idx, events);
                } else {
                    generateBookUpdate(security_id_, Side::Buy, trade_price,
                                       level.total_quantity, level.order_count,
                                       MDUpdateAction::Change, level_idx, events);
                }
            }

            if (level.empty()) {
                it = bid_levels_.erase(it);
            } else {
                ++it;
            }
        }
    }

    return events;
}

std::vector<EngineEvent> OrderBook::matchMarket(Order* order) {
    std::vector<EngineEvent> events;

    if (order->side == Side::Buy) {
        auto it = ask_levels_.begin();
        while (it != ask_levels_.end() && order->remainingQty() > 0) {
            PriceLevel& level = it->second;
            Price trade_price = it->first;

            while (!level.empty() && order->remainingQty() > 0) {
                Order* maker = level.front();
                Quantity trade_qty = std::min(order->remainingQty(), maker->remainingQty());

                Trade trade = executeTrade(maker, order, trade_price, trade_qty);

                level.total_quantity -= trade_qty;

                if (maker->isFullyFilled()) {
                    maker->status = OrdStatus::Filled;
                    level.removeOrder(maker);
                    orders_by_id_.erase(maker->order_id);
                } else {
                    maker->status = OrdStatus::PartiallyFilled;
                }

                if (order->isFullyFilled()) {
                    order->status = OrdStatus::Filled;
                } else {
                    order->status = OrdStatus::PartiallyFilled;
                }

                events.emplace_back(OrderFilled{
                    trade.trade_id,
                    trade.security_id,
                    trade.price,
                    trade.quantity,
                    trade.aggressor_side,
                    maker->order_id, maker->cl_ord_id, maker->session_uuid,
                    maker->filled_qty, maker->remainingQty(), maker->status,
                    order->order_id, order->cl_ord_id, order->session_uuid,
                    order->filled_qty, order->remainingQty(), order->status
                });

                int level_idx = priceLevelIndex(Side::Sell, trade_price);
                if (level.empty()) {
                    generateBookUpdate(security_id_, Side::Sell, trade_price,
                                       0, 0, MDUpdateAction::Delete, level_idx, events);
                } else {
                    generateBookUpdate(security_id_, Side::Sell, trade_price,
                                       level.total_quantity, level.order_count,
                                       MDUpdateAction::Change, level_idx, events);
                }
            }

            if (level.empty()) {
                it = ask_levels_.erase(it);
            } else {
                ++it;
            }
        }
    } else {
        auto it = bid_levels_.begin();
        while (it != bid_levels_.end() && order->remainingQty() > 0) {
            PriceLevel& level = it->second;
            Price trade_price = it->first;

            while (!level.empty() && order->remainingQty() > 0) {
                Order* maker = level.front();
                Quantity trade_qty = std::min(order->remainingQty(), maker->remainingQty());

                Trade trade = executeTrade(maker, order, trade_price, trade_qty);

                level.total_quantity -= trade_qty;

                if (maker->isFullyFilled()) {
                    maker->status = OrdStatus::Filled;
                    level.removeOrder(maker);
                    orders_by_id_.erase(maker->order_id);
                } else {
                    maker->status = OrdStatus::PartiallyFilled;
                }

                if (order->isFullyFilled()) {
                    order->status = OrdStatus::Filled;
                } else {
                    order->status = OrdStatus::PartiallyFilled;
                }

                events.emplace_back(OrderFilled{
                    trade.trade_id,
                    trade.security_id,
                    trade.price,
                    trade.quantity,
                    trade.aggressor_side,
                    maker->order_id, maker->cl_ord_id, maker->session_uuid,
                    maker->filled_qty, maker->remainingQty(), maker->status,
                    order->order_id, order->cl_ord_id, order->session_uuid,
                    order->filled_qty, order->remainingQty(), order->status
                });

                int level_idx = priceLevelIndex(Side::Buy, trade_price);
                if (level.empty()) {
                    generateBookUpdate(security_id_, Side::Buy, trade_price,
                                       0, 0, MDUpdateAction::Delete, level_idx, events);
                } else {
                    generateBookUpdate(security_id_, Side::Buy, trade_price,
                                       level.total_quantity, level.order_count,
                                       MDUpdateAction::Change, level_idx, events);
                }
            }

            if (level.empty()) {
                it = bid_levels_.erase(it);
            } else {
                ++it;
            }
        }
    }

    return events;
}

bool OrderBook::canFillFOK(Order* order) const {
    Quantity needed = order->quantity;

    if (order->side == Side::Buy) {
        for (const auto& [price, level] : ask_levels_) {
            if (order->order_type == OrderType::Limit && price > order->price) break;
            needed -= level.total_quantity;
            if (needed <= 0) return true;
        }
    } else {
        for (const auto& [price, level] : bid_levels_) {
            if (order->order_type == OrderType::Limit && price < order->price) break;
            needed -= level.total_quantity;
            if (needed <= 0) return true;
        }
    }

    return needed <= 0;
}

// ---------------------------------------------------------------------------
// Book manipulation
// ---------------------------------------------------------------------------

void OrderBook::insertResting(Order* order, std::vector<EngineEvent>& events) {
    orders_by_id_[order->order_id] = order;

    if (order->side == Side::Buy) {
        auto [it, inserted] = bid_levels_.try_emplace(order->price);
        if (inserted) {
            it->second.price = order->price;
        }
        it->second.addOrder(order);

        int level_idx = priceLevelIndex(Side::Buy, order->price);
        MDUpdateAction action = inserted ? MDUpdateAction::New : MDUpdateAction::Change;
        generateBookUpdate(security_id_, Side::Buy, order->price,
                           it->second.total_quantity, it->second.order_count,
                           action, level_idx, events);
    } else {
        auto [it, inserted] = ask_levels_.try_emplace(order->price);
        if (inserted) {
            it->second.price = order->price;
        }
        it->second.addOrder(order);

        int level_idx = priceLevelIndex(Side::Sell, order->price);
        MDUpdateAction action = inserted ? MDUpdateAction::New : MDUpdateAction::Change;
        generateBookUpdate(security_id_, Side::Sell, order->price,
                           it->second.total_quantity, it->second.order_count,
                           action, level_idx, events);
    }
}

void OrderBook::removeFromBook(Order* order, std::vector<EngineEvent>& events) {
    orders_by_id_.erase(order->order_id);

    if (order->side == Side::Buy) {
        auto it = bid_levels_.find(order->price);
        if (it != bid_levels_.end()) {
            int level_idx = priceLevelIndex(Side::Buy, order->price);
            it->second.removeOrder(order);
            if (it->second.empty()) {
                bid_levels_.erase(it);
                generateBookUpdate(security_id_, Side::Buy, order->price,
                                   0, 0, MDUpdateAction::Delete, level_idx, events);
            } else {
                generateBookUpdate(security_id_, Side::Buy, order->price,
                                   it->second.total_quantity, it->second.order_count,
                                   MDUpdateAction::Change, level_idx, events);
            }
        }
    } else {
        auto it = ask_levels_.find(order->price);
        if (it != ask_levels_.end()) {
            int level_idx = priceLevelIndex(Side::Sell, order->price);
            it->second.removeOrder(order);
            if (it->second.empty()) {
                ask_levels_.erase(it);
                generateBookUpdate(security_id_, Side::Sell, order->price,
                                   0, 0, MDUpdateAction::Delete, level_idx, events);
            } else {
                generateBookUpdate(security_id_, Side::Sell, order->price,
                                   it->second.total_quantity, it->second.order_count,
                                   MDUpdateAction::Change, level_idx, events);
            }
        }
    }
}

Trade OrderBook::executeTrade(Order* maker, Order* taker, Price trade_price, Quantity trade_qty) {
    maker->filled_qty += trade_qty;
    taker->filled_qty += trade_qty;

    Trade trade;
    trade.trade_id = next_trade_id_++;
    trade.security_id = security_id_;
    trade.price = trade_price;
    trade.quantity = trade_qty;
    trade.aggressor_side = taker->side;
    trade.maker_order_id = maker->order_id;
    trade.taker_order_id = taker->order_id;
    trade.maker_session_uuid = maker->session_uuid;
    trade.taker_session_uuid = taker->session_uuid;
    trade.timestamp = taker->timestamp;

    return trade;
}

int OrderBook::priceLevelIndex(Side side, Price price) const {
    int idx = 1;
    if (side == Side::Buy) {
        for (const auto& [p, level] : bid_levels_) {
            if (p == price) return idx;
            ++idx;
        }
    } else {
        for (const auto& [p, level] : ask_levels_) {
            if (p == price) return idx;
            ++idx;
        }
    }
    // New level - return the position it would occupy
    // For a new level, count how many levels are better
    idx = 1;
    if (side == Side::Buy) {
        for (const auto& [p, level] : bid_levels_) {
            if (price > p) return idx;
            ++idx;
        }
        return idx;
    } else {
        for (const auto& [p, level] : ask_levels_) {
            if (price < p) return idx;
            ++idx;
        }
        return idx;
    }
}

void OrderBook::generateBookUpdate(SecurityId sec_id, Side side, Price price,
                                    Quantity new_qty, int new_count,
                                    MDUpdateAction action, int level_idx,
                                    std::vector<EngineEvent>& events) {
    events.emplace_back(BookUpdate{
        sec_id,
        side,
        price,
        new_qty,
        new_count,
        action,
        level_idx,
        rpt_seq_++
    });
}

} // namespace cme::sim
