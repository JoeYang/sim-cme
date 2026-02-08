#pragma once
#include "../common/types.h"

namespace cme::sim {

struct Trade {
    uint64_t trade_id;
    SecurityId security_id;
    Price price;
    Quantity quantity;
    Side aggressor_side;
    OrderId maker_order_id;
    OrderId taker_order_id;
    uint64_t maker_session_uuid;
    uint64_t taker_session_uuid;
    Timestamp timestamp;
};

} // namespace cme::sim
