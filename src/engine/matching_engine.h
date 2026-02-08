#pragma once
#include "order.h"
#include "engine_event.h"
#include <vector>
#include <memory>

namespace cme::sim {

class IMatchingEngine {
public:
    virtual ~IMatchingEngine() = default;
    virtual std::vector<EngineEvent> submitOrder(std::unique_ptr<Order> order) = 0;
    virtual std::vector<EngineEvent> cancelOrder(OrderId order_id, SecurityId security_id, uint64_t session_uuid) = 0;
    virtual std::vector<EngineEvent> modifyOrder(OrderId order_id, SecurityId security_id, Price new_price, Quantity new_qty, ClOrdId new_cl_ord_id) = 0;
};

} // namespace cme::sim
