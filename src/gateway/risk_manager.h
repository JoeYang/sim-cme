#pragma once

#include "../common/types.h"
#include "../config/exchange_config.h"
#include "../engine/order.h"
#include <chrono>
#include <unordered_map>
#include <string>

namespace cme::sim::gateway {

class RiskManager {
public:
    explicit RiskManager(const config::RiskConfig& config);

    struct RiskResult {
        bool passed = true;
        std::string reason;
    };

    RiskResult checkOrder(const Order& order) const;
    RiskResult checkRate(uint64_t session_uuid);

    void onFill(uint64_t session_uuid, SecurityId security_id,
                Side side, Quantity qty);

private:
    config::RiskConfig config_;

    struct SessionRiskState {
        int64_t net_position = 0;
        std::chrono::steady_clock::time_point last_order_time;
        int orders_this_second = 0;
    };
    std::unordered_map<uint64_t, SessionRiskState> session_state_;
};

} // namespace cme::sim::gateway
