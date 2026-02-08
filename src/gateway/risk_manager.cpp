#include "risk_manager.h"
#include <cmath>

namespace cme::sim::gateway {

RiskManager::RiskManager(const config::RiskConfig& config)
    : config_(config) {}

RiskManager::RiskResult RiskManager::checkOrder(const Order& order) const {
    RiskResult result;

    // Max order quantity check
    if (order.quantity > config_.max_order_qty) {
        result.passed = false;
        result.reason = "Order quantity " + std::to_string(order.quantity) +
                        " exceeds max " + std::to_string(config_.max_order_qty);
        return result;
    }

    return result;
}

RiskManager::RiskResult RiskManager::checkRate(uint64_t session_uuid) {
    RiskResult result;
    auto now = std::chrono::steady_clock::now();
    auto& state = session_state_[session_uuid];

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - state.last_order_time);

    if (elapsed.count() >= 1) {
        // New second window
        state.last_order_time = now;
        state.orders_this_second = 1;
    } else {
        state.orders_this_second++;
        if (state.orders_this_second > config_.max_orders_per_second) {
            result.passed = false;
            result.reason = "Rate limit exceeded: " +
                            std::to_string(state.orders_this_second) +
                            " orders/sec (max " +
                            std::to_string(config_.max_orders_per_second) + ")";
            return result;
        }
    }

    return result;
}

void RiskManager::onFill(uint64_t session_uuid, SecurityId /*security_id*/,
                         Side side, Quantity qty) {
    auto& state = session_state_[session_uuid];
    if (side == Side::Buy) {
        state.net_position += qty;
    } else {
        state.net_position -= qty;
    }
}

} // namespace cme::sim::gateway
