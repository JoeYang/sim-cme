#pragma once

#include "../common/types.h"
#include <functional>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace cme::sim {

// Event emitted when an instrument's trading status changes
struct SecurityStatusEvent {
    SecurityId security_id;
    SecurityTradingStatus old_status;
    SecurityTradingStatus new_status;
    Timestamp timestamp;
};

using SecurityStatusCallback = std::function<void(const SecurityStatusEvent&)>;

class SecurityStatusMachine {
public:
    // Register an instrument with an initial status
    void addInstrument(SecurityId id, SecurityTradingStatus initial = SecurityTradingStatus::PreOpen);

    // Get the current status of an instrument
    SecurityTradingStatus getStatus(SecurityId id) const;

    // Attempt a transition; returns true if the transition was valid.
    // Fires the registered callback on success.
    bool transition(SecurityId id, SecurityTradingStatus new_status);

    // Convenience helpers for common transitions
    bool openMarket(SecurityId id);
    bool haltTrading(SecurityId id);
    bool resumeTrading(SecurityId id);
    bool closeMarket(SecurityId id);

    // Bulk operations: apply the same transition to every registered instrument
    void openAll();
    void closeAll();

    // Register a callback that fires on any status change
    void setCallback(SecurityStatusCallback cb);

private:
    std::unordered_map<SecurityId, SecurityTradingStatus> states_;
    SecurityStatusCallback callback_;

    // Returns true if from -> to is a valid transition
    static bool isValidTransition(SecurityTradingStatus from, SecurityTradingStatus to);

    Timestamp now() const;
};

} // namespace cme::sim
