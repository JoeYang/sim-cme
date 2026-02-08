#pragma once

#include <cstdint>
#include <climits>
#include <string>
#include <chrono>

namespace cme::sim {

// ---------------------------------------------------------------------------
// Scalar type aliases
// ---------------------------------------------------------------------------
using OrderId    = uint64_t;
using ClOrdId    = std::string;
using SecurityId = int32_t;
using SeqNum     = uint32_t;
using Timestamp  = uint64_t; // nanoseconds since epoch

// ---------------------------------------------------------------------------
// Fixed-point price (PRICENULL9: mantissa * 10^-9)
// ---------------------------------------------------------------------------
struct Price {
    int64_t mantissa = 0;
    static constexpr int8_t  exponent   = -9;
    static constexpr int64_t NULL_VALUE = INT64_MAX;

    bool   isNull()   const { return mantissa == NULL_VALUE; }
    double toDouble() const { return mantissa * 1e-9; }

    static Price fromDouble(double d) {
        return {static_cast<int64_t>(d * 1e9)};
    }
    static Price null() { return {NULL_VALUE}; }

    bool operator< (const Price& o) const { return mantissa <  o.mantissa; }
    bool operator> (const Price& o) const { return mantissa >  o.mantissa; }
    bool operator==(const Price& o) const { return mantissa == o.mantissa; }
    bool operator!=(const Price& o) const { return mantissa != o.mantissa; }
    bool operator<=(const Price& o) const { return mantissa <= o.mantissa; }
    bool operator>=(const Price& o) const { return mantissa >= o.mantissa; }

    Price operator+(const Price& o) const { return {mantissa + o.mantissa}; }
    Price operator-(const Price& o) const { return {mantissa - o.mantissa}; }
};

using Quantity = int32_t;

// ---------------------------------------------------------------------------
// Enumerations  (values match CME iLink 3 / MDP 3.0 wire encoding)
// ---------------------------------------------------------------------------
enum class Side : uint8_t {
    Buy  = 1,
    Sell = 2
};

enum class OrderType : uint8_t {
    Market    = 1,
    Limit     = 2,
    StopLimit = 3,
    StopMarket = 4
};

enum class TimeInForce : uint8_t {
    Day = 0,
    GTC = 1,
    IOC = 3,
    FOK = 4,
    GTD = 6
};

enum class OrdStatus : uint8_t {
    New             = 0,
    PartiallyFilled = 1,
    Filled          = 2,
    Canceled        = 4,
    Replaced        = 5,
    Rejected        = 8
};

enum class ExecType : char {
    New      = '0',
    Canceled = '4',
    Replaced = '5',
    Trade    = 'F',
    Rejected = '8'
};

enum class MDUpdateAction : uint8_t {
    New       = 0,
    Change    = 1,
    Delete    = 2,
    DeleteThru = 3,
    DeleteFrom = 4,
    Overlay   = 5
};

enum class MDEntryType : char {
    Bid   = '0',
    Offer = '1',
    Trade = '2'
};

enum class SecurityTradingStatus : uint8_t {
    PreOpen = 2,
    Open    = 17,
    Halt    = 18,
    Close   = 21
};

enum class MatchEventIndicator : uint8_t {
    LastTradeMsg   = 0x01,
    LastVolumeMsg  = 0x02,
    LastQuoteMsg   = 0x04,
    LastStatsMsg   = 0x08,
    LastImpliedMsg = 0x10,
    RecoveryMsg    = 0x20,
    Reserved       = 0x40,
    EndOfEvent     = 0x80
};

// Bitwise OR for MatchEventIndicator
inline MatchEventIndicator operator|(MatchEventIndicator a, MatchEventIndicator b) {
    return static_cast<MatchEventIndicator>(
        static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline MatchEventIndicator operator&(MatchEventIndicator a, MatchEventIndicator b) {
    return static_cast<MatchEventIndicator>(
        static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

} // namespace cme::sim
