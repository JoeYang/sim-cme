#pragma once

#include "session.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace cme::sim::fixp {

class SessionManager {
public:
    explicit SessionManager(size_t max_sessions = 100);

    // Create a new session for a TCP connection. Returns nullptr if at capacity.
    std::shared_ptr<Session> createSession(SendCallback send_cb, AppMessageCallback app_cb);

    // Remove a session by UUID
    void removeSession(uint64_t uuid);

    // Find session by UUID
    std::shared_ptr<Session> findSession(uint64_t uuid);

    // Timer tick for all sessions (call periodically from event loop)
    void onTimerTick();

    size_t activeSessionCount() const;

private:
    std::unordered_map<uint64_t, std::shared_ptr<Session>> sessions_;
    uint64_t next_uuid_ = 1;
    size_t max_sessions_;
    mutable std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace cme::sim::fixp
