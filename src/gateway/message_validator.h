#pragma once

#include "../common/types.h"
#include "../instruments/instrument_manager.h"
#include "../engine/order.h"
#include <string>

namespace cme::sim::gateway {

class MessageValidator {
public:
    explicit MessageValidator(const InstrumentManager& instrument_mgr);

    struct ValidationResult {
        bool valid = true;
        std::string reason;
        uint16_t reject_reason = 0; // OrdRejReason
    };

    ValidationResult validateNewOrder(const Order& order) const;
    ValidationResult validateCancel(OrderId order_id, SecurityId security_id) const;
    ValidationResult validateModify(OrderId order_id, SecurityId security_id,
                                    Price new_price, Quantity new_qty) const;

private:
    const InstrumentManager& instrument_mgr_;

    bool isValidInstrument(SecurityId id) const;
    bool isValidPrice(SecurityId id, Price price) const;
    bool isValidQuantity(SecurityId id, Quantity qty) const;
    bool isValidOrderType(OrderType type) const;
    bool isValidTimeInForce(TimeInForce tif, OrderType type) const;
};

} // namespace cme::sim::gateway
