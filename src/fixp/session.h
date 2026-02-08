#pragma once

#include "../sbe/ilink3_messages.h"
#include "../sbe/framing.h"
#include "../sbe/message_header.h"
#include "../common/types.h"
#include "../common/logger.h"
#include "retransmit_buffer.h"
#include "hmac_authenticator.h"
#include <functional>
#include <vector>
#include <memory>
#include <string>
#include <chrono>

namespace cme::sim::fixp {

enum class SessionState {
    Connected,      // TCP connected, awaiting Negotiate
    Negotiated,     // Negotiate completed, awaiting Establish
    Established,    // Active session, application messages flow
    Terminated      // Session ended
};

inline const char* sessionStateToString(SessionState s) {
    switch (s) {
        case SessionState::Connected:   return "Connected";
        case SessionState::Negotiated:  return "Negotiated";
        case SessionState::Established: return "Established";
        case SessionState::Terminated:  return "Terminated";
    }
    return "Unknown";
}

// Callback for sending data back to the TCP connection
using SendCallback = std::function<void(const char* data, size_t len)>;
// Callback for forwarding application messages to the gateway
using AppMessageCallback = std::function<void(uint64_t uuid, uint16_t templateId, const char* data, size_t len)>;

class Session {
public:
    Session(uint64_t assigned_uuid, SendCallback send_cb, AppMessageCallback app_cb);

    // Process an incoming SBE message (after SOFH is stripped by TCP layer).
    // data points to the SBE MessageHeader.
    void onMessage(const char* data, size_t len);

    // Timer-driven: check keepalive, send heartbeat if needed
    void onTimer();

    // Send an application message (wraps in SOFH, manages seq nums).
    // sbe_data must be a fully-encoded SBE message (header + body).
    void sendApplicationMessage(const char* sbe_data, size_t sbe_len);

    // Session info
    uint64_t uuid() const { return uuid_; }
    SessionState state() const { return state_; }
    uint32_t nextOutSeqNo() const { return next_out_seq_; }

    // Graceful terminate
    void terminate(uint16_t error_code = 0);

    // HMAC configuration
    void setHmacKey(const std::string& key);
    void setHmacEnabled(bool enabled);

private:
    uint64_t uuid_;
    SessionState state_ = SessionState::Connected;
    SendCallback send_cb_;
    AppMessageCallback app_cb_;

    uint32_t next_in_seq_ = 1;
    uint32_t next_out_seq_ = 1;
    uint32_t keep_alive_interval_ms_ = 30000;

    std::chrono::steady_clock::time_point last_received_;
    std::chrono::steady_clock::time_point last_sent_;

    bool hmac_enabled_ = false;
    std::string hmac_key_;

    RetransmitBuffer retransmit_buffer_;

    std::shared_ptr<spdlog::logger> logger_;

    // Message handlers
    void handleNegotiate(const char* data, size_t len);
    void handleEstablish(const char* data, size_t len);
    void handleSequence(const char* data, size_t len);
    void handleTerminate(const char* data, size_t len);
    void handleRetransmitRequest(const char* data, size_t len);
    void handleApplicationMessage(uint16_t templateId, const char* data, size_t len);

    // Send helpers -- build SBE, frame with SOFH, and call send_cb_
    void sendFramedMessage(const char* sbe_data, size_t sbe_len);
    void sendNegotiationResponse(uint64_t request_timestamp);
    void sendEstablishmentAck(uint64_t request_timestamp, uint16_t keep_alive_interval);
    void sendSequenceHeartbeat();
    void sendTerminate(uint16_t error_code);
    void sendNotApplied(uint32_t from_seq, uint32_t msg_count);
    void sendRetransmission(uint64_t last_uuid, uint64_t request_timestamp,
                            uint32_t from_seq, uint16_t msg_count);

    // HMAC verification
    bool verifyHmac(const char* hmac_signature, const char* data, size_t len);

    // Build SOFH-framed message from SBE payload
    static std::vector<char> frameMessage(const char* sbe_data, size_t sbe_len);

    // Current time as nanoseconds since epoch
    static uint64_t nowNanos();
};

} // namespace cme::sim::fixp
