#pragma once

#include "../common/types.h"
#include <string>
#include <cstdint>

namespace cme::sim {

struct Instrument {
    SecurityId security_id;
    std::string symbol;            // e.g. "ESH5"
    std::string security_group;    // e.g. "ES"
    std::string asset;             // e.g. "ES"
    int channel_id;

    // Pricing
    double tick_size;              // minimum price increment (e.g. 0.25 for ES)
    double contract_multiplier;    // e.g. 50 for ES
    double min_price_increment_amount; // tick_size * multiplier
    double display_factor;         // e.g. 0.01

    // Quantity limits
    Quantity min_trade_vol = 1;
    Quantity max_trade_vol = 10000;

    // Contract info
    std::string maturity_month_year; // e.g. "202503"
    std::string unit_of_measure = "Qty";

    // Trading status
    SecurityTradingStatus trading_status = SecurityTradingStatus::PreOpen;

    // Convert a floating-point tick size to mantissa in the Price fixed-point space
    int64_t tickMantissa() const {
        return static_cast<int64_t>(tick_size * 1e9);
    }

    // Round a price to the nearest valid tick
    Price roundToTick(Price price) const {
        int64_t tm = tickMantissa();
        if (tm == 0) return price;
        int64_t remainder = price.mantissa % tm;
        if (remainder == 0) return price;
        if (remainder < 0) remainder += tm; // handle negative prices
        if (remainder > tm / 2)
            return Price{price.mantissa + (tm - remainder)};
        else
            return Price{price.mantissa - remainder};
    }

    // Check whether a price falls on a valid tick boundary
    bool isValidTick(Price price) const {
        int64_t tm = tickMantissa();
        if (tm == 0) return true;
        return (price.mantissa % tm) == 0;
    }

    // Convert a number of ticks to a Price delta
    Price ticksToPrice(int64_t ticks) const {
        return Price{ticks * tickMantissa()};
    }

    // Convert a Price delta to number of ticks
    int64_t priceToTicks(Price price) const {
        int64_t tm = tickMantissa();
        if (tm == 0) return 0;
        return price.mantissa / tm;
    }
};

} // namespace cme::sim
