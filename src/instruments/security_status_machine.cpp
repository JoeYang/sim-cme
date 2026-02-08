#include "security_status_machine.h"
#include <chrono>

namespace cme::sim {

void SecurityStatusMachine::addInstrument(SecurityId id, SecurityTradingStatus initial) {
    states_[id] = initial;
}

SecurityTradingStatus SecurityStatusMachine::getStatus(SecurityId id) const {
    auto it = states_.find(id);
    if (it == states_.end()) return SecurityTradingStatus::PreOpen;
    return it->second;
}

bool SecurityStatusMachine::transition(SecurityId id, SecurityTradingStatus new_status) {
    auto it = states_.find(id);
    if (it == states_.end()) return false;

    SecurityTradingStatus old_status = it->second;
    if (old_status == new_status) return true; // no-op

    if (!isValidTransition(old_status, new_status)) return false;

    it->second = new_status;

    if (callback_) {
        SecurityStatusEvent event;
        event.security_id = id;
        event.old_status = old_status;
        event.new_status = new_status;
        event.timestamp = now();
        callback_(event);
    }

    return true;
}

bool SecurityStatusMachine::openMarket(SecurityId id) {
    return transition(id, SecurityTradingStatus::Open);
}

bool SecurityStatusMachine::haltTrading(SecurityId id) {
    return transition(id, SecurityTradingStatus::Halt);
}

bool SecurityStatusMachine::resumeTrading(SecurityId id) {
    // Halt -> Open
    return transition(id, SecurityTradingStatus::Open);
}

bool SecurityStatusMachine::closeMarket(SecurityId id) {
    return transition(id, SecurityTradingStatus::Close);
}

void SecurityStatusMachine::openAll() {
    for (auto& [id, status] : states_) {
        transition(id, SecurityTradingStatus::Open);
    }
}

void SecurityStatusMachine::closeAll() {
    for (auto& [id, status] : states_) {
        transition(id, SecurityTradingStatus::Close);
    }
}

void SecurityStatusMachine::setCallback(SecurityStatusCallback cb) {
    callback_ = std::move(cb);
}

bool SecurityStatusMachine::isValidTransition(SecurityTradingStatus from, SecurityTradingStatus to) {
    // Valid transitions:
    //   PreOpen -> Open
    //   Open    -> Halt
    //   Open    -> Close
    //   Halt    -> Open  (resume)
    //   Halt    -> Close
    switch (from) {
        case SecurityTradingStatus::PreOpen:
            return to == SecurityTradingStatus::Open;
        case SecurityTradingStatus::Open:
            return to == SecurityTradingStatus::Halt || to == SecurityTradingStatus::Close;
        case SecurityTradingStatus::Halt:
            return to == SecurityTradingStatus::Open || to == SecurityTradingStatus::Close;
        case SecurityTradingStatus::Close:
            return false; // terminal state
        default:
            return false;
    }
}

Timestamp SecurityStatusMachine::now() const {
    auto tp = std::chrono::system_clock::now();
    return static_cast<Timestamp>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            tp.time_since_epoch()).count());
}

} // namespace cme::sim
