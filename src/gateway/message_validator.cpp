#include "message_validator.h"

namespace cme::sim::gateway {

MessageValidator::MessageValidator(const InstrumentManager& instrument_mgr)
    : instrument_mgr_(instrument_mgr) {}

MessageValidator::ValidationResult MessageValidator::validateNewOrder(const Order& order) const {
    ValidationResult result;

    // Check instrument exists
    if (!isValidInstrument(order.security_id)) {
        result.valid = false;
        result.reason = "Unknown instrument";
        result.reject_reason = 2; // UnknownSymbol
        return result;
    }

    // Check instrument is open for trading
    const auto* instr = instrument_mgr_.findBySecurityId(order.security_id);
    if (instr->trading_status != SecurityTradingStatus::Open &&
        instr->trading_status != SecurityTradingStatus::PreOpen) {
        result.valid = false;
        result.reason = "Instrument not available for trading";
        result.reject_reason = 16; // ExchangeClosed
        return result;
    }

    // Check order type
    if (!isValidOrderType(order.order_type)) {
        result.valid = false;
        result.reason = "Unsupported order type";
        result.reject_reason = 11; // UnsupportedOrderCharacteristic
        return result;
    }

    // Check TIF
    if (!isValidTimeInForce(order.time_in_force, order.order_type)) {
        result.valid = false;
        result.reason = "Invalid TimeInForce for order type";
        result.reject_reason = 11;
        return result;
    }

    // Check price validity for limit orders
    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLimit) {
        if (!isValidPrice(order.security_id, order.price)) {
            result.valid = false;
            result.reason = "Price not on valid tick";
            result.reject_reason = 15; // InvalidPriceIncrement
            return result;
        }
    }

    // Check quantity
    if (!isValidQuantity(order.security_id, order.quantity)) {
        result.valid = false;
        result.reason = "Quantity outside allowed range";
        result.reject_reason = 13; // IncorrectQuantity
        return result;
    }

    return result;
}

MessageValidator::ValidationResult MessageValidator::validateCancel(
    OrderId order_id, SecurityId security_id) const {

    ValidationResult result;

    if (order_id == 0) {
        result.valid = false;
        result.reason = "OrderId required for cancel";
        result.reject_reason = 99;
        return result;
    }

    if (!isValidInstrument(security_id)) {
        result.valid = false;
        result.reason = "Unknown instrument";
        result.reject_reason = 2;
        return result;
    }

    return result;
}

MessageValidator::ValidationResult MessageValidator::validateModify(
    OrderId order_id, SecurityId security_id,
    Price new_price, Quantity new_qty) const {

    ValidationResult result;

    if (order_id == 0) {
        result.valid = false;
        result.reason = "OrderId required for modify";
        result.reject_reason = 99;
        return result;
    }

    if (!isValidInstrument(security_id)) {
        result.valid = false;
        result.reason = "Unknown instrument";
        result.reject_reason = 2;
        return result;
    }

    if (!new_price.isNull() && !isValidPrice(security_id, new_price)) {
        result.valid = false;
        result.reason = "New price not on valid tick";
        result.reject_reason = 15;
        return result;
    }

    if (new_qty > 0 && !isValidQuantity(security_id, new_qty)) {
        result.valid = false;
        result.reason = "New quantity outside allowed range";
        result.reject_reason = 13;
        return result;
    }

    return result;
}

bool MessageValidator::isValidInstrument(SecurityId id) const {
    return instrument_mgr_.findBySecurityId(id) != nullptr;
}

bool MessageValidator::isValidPrice(SecurityId id, Price price) const {
    const auto* instr = instrument_mgr_.findBySecurityId(id);
    if (!instr) return false;
    return instr->isValidTick(price);
}

bool MessageValidator::isValidQuantity(SecurityId id, Quantity qty) const {
    if (qty <= 0) return false;
    const auto* instr = instrument_mgr_.findBySecurityId(id);
    if (!instr) return false;
    return qty >= instr->min_trade_vol && qty <= instr->max_trade_vol;
}

bool MessageValidator::isValidOrderType(OrderType type) const {
    switch (type) {
        case OrderType::Market:
        case OrderType::Limit:
        case OrderType::StopLimit:
        case OrderType::StopMarket:
            return true;
        default:
            return false;
    }
}

bool MessageValidator::isValidTimeInForce(TimeInForce tif, OrderType type) const {
    switch (tif) {
        case TimeInForce::Day:
        case TimeInForce::GTC:
        case TimeInForce::GTD:
            return true;
        case TimeInForce::IOC:
        case TimeInForce::FOK:
            // IOC/FOK valid for Limit and Market orders
            return type == OrderType::Limit || type == OrderType::Market;
        default:
            return false;
    }
}

} // namespace cme::sim::gateway
