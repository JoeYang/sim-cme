#include "full_matching_engine.h"
#include <chrono>

namespace cme::sim {

FullMatchingEngine::FullMatchingEngine() = default;

void FullMatchingEngine::addInstrument(SecurityId security_id) {
    order_books_.try_emplace(security_id, security_id);
}

std::vector<EngineEvent> FullMatchingEngine::submitOrder(std::unique_ptr<Order> order) {
    // Assign order ID and timestamp
    order->order_id = next_order_id_++;
    order->timestamp = static_cast<Timestamp>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    SecurityId sec_id = order->security_id;

    // Find the order book
    auto book_it = order_books_.find(sec_id);
    if (book_it == order_books_.end()) {
        std::vector<EngineEvent> events;
        events.emplace_back(OrderRejected{
            order->cl_ord_id,
            order->session_uuid,
            "Unknown security ID",
            0
        });
        return events;
    }

    OrderId oid = order->order_id;
    Order* raw_ptr = order.get();

    // Take ownership
    owned_orders_[oid] = std::move(order);
    order_to_security_[oid] = sec_id;

    auto events = book_it->second.addOrder(raw_ptr);

    // Clean up fully filled or cancelled orders that aren't resting
    if (raw_ptr->isFullyFilled() || raw_ptr->status == OrdStatus::Canceled ||
        raw_ptr->status == OrdStatus::Rejected) {
        // The order book won't hold a reference to these, so we can keep
        // them in owned_orders_ for potential query, or clean up.
        // For cancelled IOC/FOK, remove from tracking maps.
        order_to_security_.erase(oid);
        owned_orders_.erase(oid);
    }

    return events;
}

std::vector<EngineEvent> FullMatchingEngine::cancelOrder(OrderId order_id, SecurityId security_id, uint64_t session_uuid) {
    // Look up the security for this order
    auto sec_it = order_to_security_.find(order_id);
    SecurityId sec_id = security_id;
    if (sec_it != order_to_security_.end()) {
        sec_id = sec_it->second;
    }

    auto book_it = order_books_.find(sec_id);
    if (book_it == order_books_.end()) {
        std::vector<EngineEvent> events;
        events.emplace_back(OrderCancelRejected{
            order_id,
            ClOrdId{},
            session_uuid,
            0,
            "Unknown security ID"
        });
        return events;
    }

    auto events = book_it->second.cancelOrder(order_id);

    // Clean up ownership
    order_to_security_.erase(order_id);
    owned_orders_.erase(order_id);

    return events;
}

std::vector<EngineEvent> FullMatchingEngine::modifyOrder(OrderId order_id, SecurityId security_id, Price new_price, Quantity new_qty, ClOrdId new_cl_ord_id) {
    auto sec_it = order_to_security_.find(order_id);
    SecurityId sec_id = security_id;
    if (sec_it != order_to_security_.end()) {
        sec_id = sec_it->second;
    }

    auto book_it = order_books_.find(sec_id);
    if (book_it == order_books_.end()) {
        std::vector<EngineEvent> events;
        events.emplace_back(OrderCancelRejected{
            order_id,
            new_cl_ord_id,
            0,
            0,
            "Unknown security ID"
        });
        return events;
    }

    auto events = book_it->second.modifyOrder(order_id, new_price, new_qty, new_cl_ord_id);

    // If the order was fully filled after re-matching, clean up
    auto owned_it = owned_orders_.find(order_id);
    if (owned_it != owned_orders_.end()) {
        Order* raw_ptr = owned_it->second.get();
        if (raw_ptr->isFullyFilled() || raw_ptr->status == OrdStatus::Canceled) {
            order_to_security_.erase(order_id);
            owned_orders_.erase(order_id);
        }
    }

    return events;
}

const OrderBook* FullMatchingEngine::getOrderBook(SecurityId security_id) const {
    auto it = order_books_.find(security_id);
    if (it == order_books_.end()) return nullptr;
    return &it->second;
}

} // namespace cme::sim
