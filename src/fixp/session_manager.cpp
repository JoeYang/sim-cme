#include "session_manager.h"

namespace cme::sim::fixp {

SessionManager::SessionManager(size_t max_sessions)
    : max_sessions_(max_sessions)
    , logger_(getLogger(LogCategory::FIXP))
{
}

std::shared_ptr<Session> SessionManager::createSession(
    SendCallback send_cb, AppMessageCallback app_cb)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (sessions_.size() >= max_sessions_) {
        logger_->warn("SessionManager: max sessions ({}) reached, rejecting new session",
                      max_sessions_);
        return nullptr;
    }

    uint64_t uuid = next_uuid_++;
    auto session = std::make_shared<Session>(uuid, std::move(send_cb), std::move(app_cb));
    sessions_[uuid] = session;

    logger_->info("SessionManager: created session UUID={} (active={})",
                  uuid, sessions_.size());
    return session;
}

void SessionManager::removeSession(uint64_t uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(uuid);
    if (it != sessions_.end()) {
        sessions_.erase(it);
        logger_->info("SessionManager: removed session UUID={} (active={})",
                      uuid, sessions_.size());
    }
}

std::shared_ptr<Session> SessionManager::findSession(uint64_t uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(uuid);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

void SessionManager::onTimerTick() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Collect terminated sessions for cleanup
    std::vector<uint64_t> terminated;

    for (auto& [uuid, session] : sessions_) {
        session->onTimer();
        if (session->state() == SessionState::Terminated) {
            terminated.push_back(uuid);
        }
    }

    for (uint64_t uuid : terminated) {
        sessions_.erase(uuid);
        logger_->info("SessionManager: cleaned up terminated session UUID={} (active={})",
                      uuid, sessions_.size());
    }
}

size_t SessionManager::activeSessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

} // namespace cme::sim::fixp
