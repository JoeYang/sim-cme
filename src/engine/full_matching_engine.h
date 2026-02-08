#pragma once
#include "matching_engine.h"
#include "order_book.h"
#include <unordered_map>
#include <memory>

namespace cme::sim {

class FullMatchingEngine : public IMatchingEngine {
public:
    FullMatchingEngine();

    void addInstrument(SecurityId security_id);

    std::vector<EngineEvent> submitOrder(std::unique_ptr<Order> order) override;
    std::vector<EngineEvent> cancelOrder(OrderId order_id, SecurityId security_id, uint64_t session_uuid) override;
    std::vector<EngineEvent> modifyOrder(OrderId order_id, SecurityId security_id, Price new_price, Quantity new_qty, ClOrdId new_cl_ord_id) override;

    const OrderBook* getOrderBook(SecurityId security_id) const;

private:
    std::unordered_map<SecurityId, OrderBook> order_books_;
    std::unordered_map<OrderId, SecurityId> order_to_security_;
    // Owns all resting and in-flight orders
    std::unordered_map<OrderId, std::unique_ptr<Order>> owned_orders_;
    uint64_t next_order_id_ = 1;
};

} // namespace cme::sim
