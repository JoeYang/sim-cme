#pragma once
#include "../common/types.h"
#include <string>

namespace cme::sim {

struct Order {
    OrderId order_id = 0;
    ClOrdId cl_ord_id;
    uint64_t session_uuid = 0;  // owning session
    SecurityId security_id = 0;
    Side side = Side::Buy;
    OrderType order_type = OrderType::Limit;
    TimeInForce time_in_force = TimeInForce::Day;
    Price price;
    Price stop_price;
    Quantity quantity = 0;
    Quantity filled_qty = 0;
    Quantity display_qty = 0;
    Quantity min_qty = 0;
    Timestamp timestamp = 0;
    OrdStatus status = OrdStatus::New;
    uint64_t order_request_id = 0;

    Quantity remainingQty() const { return quantity - filled_qty; }
    bool isFullyFilled() const { return filled_qty >= quantity; }

    // Intrusive list pointers for price level
    Order* prev_in_level = nullptr;
    Order* next_in_level = nullptr;
};

} // namespace cme::sim
