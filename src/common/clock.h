#pragma once

#include <chrono>
#include <cstdint>

namespace cme::sim {

// ---------------------------------------------------------------------------
// High-resolution clock utilities
// ---------------------------------------------------------------------------
class Clock {
public:
    using SteadyClock = std::chrono::steady_clock;
    using SystemClock = std::chrono::system_clock;

    // Nanoseconds since Unix epoch (wall-clock time)
    static uint64_t epochNanos() {
        auto now = SystemClock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch())
                .count());
    }

    // Monotonic nanosecond timestamp (for latency measurement)
    static uint64_t steadyNanos() {
        auto now = SteadyClock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch())
                .count());
    }

    // Monotonic microsecond timestamp
    static uint64_t steadyMicros() {
        auto now = SteadyClock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch())
                .count());
    }

    // Milliseconds since epoch (wall-clock)
    static uint64_t epochMillis() {
        auto now = SystemClock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count());
    }

    // Duration helpers
    static uint64_t nanosToMillis(uint64_t nanos)  { return nanos / 1'000'000ULL; }
    static uint64_t millisToNanos(uint64_t millis)  { return millis * 1'000'000ULL; }
    static uint64_t nanosToMicros(uint64_t nanos)   { return nanos / 1'000ULL; }
};

} // namespace cme::sim
