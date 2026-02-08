#pragma once
#include "../common/types.h"
#include <string>
#include <variant>

namespace cme::sim {

struct OrderAccepted {
    OrderId order_id;
    ClOrdId cl_ord_id;
    uint64_t session_uuid;
    SecurityId security_id;
    Side side;
    Price price;
    Quantity quantity;
    OrderType order_type;
    TimeInForce time_in_force;
};

struct OrderRejected {
    ClOrdId cl_ord_id;
    uint64_t session_uuid;
    std::string reason;
    uint16_t reject_reason_code = 0;
};

struct OrderFilled {
    uint64_t trade_id;
    SecurityId security_id;
    Price trade_price;
    Quantity trade_qty;
    Side aggressor_side;

    // Maker side
    OrderId maker_order_id;
    ClOrdId maker_cl_ord_id;
    uint64_t maker_session_uuid;
    Quantity maker_cum_qty;
    Quantity maker_leaves_qty;
    OrdStatus maker_ord_status;

    // Taker side
    OrderId taker_order_id;
    ClOrdId taker_cl_ord_id;
    uint64_t taker_session_uuid;
    Quantity taker_cum_qty;
    Quantity taker_leaves_qty;
    OrdStatus taker_ord_status;
};

struct OrderCancelled {
    OrderId order_id;
    ClOrdId cl_ord_id;
    uint64_t session_uuid;
    SecurityId security_id;
    Quantity cum_qty;
    OrdStatus ord_status;
};

struct OrderModified {
    OrderId order_id;
    ClOrdId cl_ord_id;
    uint64_t session_uuid;
    SecurityId security_id;
    Price new_price;
    Quantity new_qty;
    Quantity cum_qty;
    Quantity leaves_qty;
};

struct OrderCancelRejected {
    OrderId order_id;
    ClOrdId cl_ord_id;
    uint64_t session_uuid;
    uint16_t reject_reason_code = 0;
    std::string reason;
};

struct BookUpdate {
    SecurityId security_id;
    Side side;
    Price price;
    Quantity new_qty;
    int new_order_count;
    MDUpdateAction update_action;
    int price_level_index;  // 1-based
    uint32_t rpt_seq;
};

using EngineEvent = std::variant<
    OrderAccepted,
    OrderRejected,
    OrderFilled,
    OrderCancelled,
    OrderModified,
    OrderCancelRejected,
    BookUpdate
>;

} // namespace cme::sim
