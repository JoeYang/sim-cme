#include "session.h"
#include <cstring>

namespace cme::sim::fixp {

Session::Session(uint64_t assigned_uuid, SendCallback send_cb, AppMessageCallback app_cb)
    : uuid_(assigned_uuid)
    , send_cb_(std::move(send_cb))
    , app_cb_(std::move(app_cb))
    , last_received_(std::chrono::steady_clock::now())
    , last_sent_(std::chrono::steady_clock::now())
    , retransmit_buffer_(10000)
    , logger_(getLogger(LogCategory::FIXP))
{
    logger_->info("Session created with UUID={}", uuid_);
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void Session::onMessage(const char* data, size_t len) {
    if (len < sbe::MessageHeader::SIZE) {
        logger_->warn("UUID={}: message too small ({}B), ignoring", uuid_, len);
        return;
    }

    last_received_ = std::chrono::steady_clock::now();

    uint16_t templateId = sbe::MessageHeader::decodeTemplateId(data);

    switch (templateId) {
        case sbe::Negotiate500::TEMPLATE_ID:
            handleNegotiate(data, len);
            break;
        case sbe::Establish503::TEMPLATE_ID:
            handleEstablish(data, len);
            break;
        case sbe::Sequence506::TEMPLATE_ID:
            handleSequence(data, len);
            break;
        case sbe::Terminate507::TEMPLATE_ID:
            handleTerminate(data, len);
            break;
        case sbe::RetransmitRequest508::TEMPLATE_ID:
            handleRetransmitRequest(data, len);
            break;
        default:
            if (templateId >= 514) {
                handleApplicationMessage(templateId, data, len);
            } else {
                logger_->warn("UUID={}: unknown templateId={} in state {}, ignoring",
                              uuid_, templateId, sessionStateToString(state_));
            }
            break;
    }
}

void Session::onTimer() {
    if (state_ != SessionState::Established) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto since_sent = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_sent_).count();
    auto since_recv = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_received_).count();

    // Send heartbeat if we haven't sent anything recently
    if (since_sent >= static_cast<int64_t>(keep_alive_interval_ms_)) {
        sendSequenceHeartbeat();
    }

    // If the client hasn't sent anything in 2x keepalive, terminate
    if (since_recv >= static_cast<int64_t>(keep_alive_interval_ms_) * 2) {
        logger_->warn("UUID={}: keepalive timeout ({}ms since last recv), terminating",
                      uuid_, since_recv);
        terminate(1); // error code 1 = keepalive violation
    }
}

void Session::sendApplicationMessage(const char* sbe_data, size_t sbe_len) {
    if (state_ != SessionState::Established) {
        logger_->warn("UUID={}: cannot send app message in state {}",
                      uuid_, sessionStateToString(state_));
        return;
    }

    // Store for retransmission
    retransmit_buffer_.store(next_out_seq_, sbe_data, sbe_len);
    ++next_out_seq_;

    sendFramedMessage(sbe_data, sbe_len);
}

void Session::terminate(uint16_t error_code) {
    if (state_ == SessionState::Terminated) {
        return;
    }
    logger_->info("UUID={}: terminating with error_code={}", uuid_, error_code);
    sendTerminate(error_code);
    state_ = SessionState::Terminated;
}

void Session::setHmacKey(const std::string& key) {
    hmac_key_ = key;
}

void Session::setHmacEnabled(bool enabled) {
    hmac_enabled_ = enabled;
}

// ---------------------------------------------------------------------------
// Message handlers
// ---------------------------------------------------------------------------

void Session::handleNegotiate(const char* data, size_t len) {
    if (state_ != SessionState::Connected) {
        logger_->warn("UUID={}: Negotiate500 received in state {}, ignoring",
                      uuid_, sessionStateToString(state_));
        return;
    }

    sbe::Negotiate500 neg;
    neg.decode(data, 0);

    logger_->info("UUID={}: Negotiate500 received (client UUID={}, sendingTime={})",
                  uuid_, neg.uuid, neg.sendingTime);

    // Verify HMAC if enabled
    if (hmac_enabled_) {
        // HMAC is computed over the message body after the 32-byte signature field.
        // The signature is at the start of the body (offset 0 after MessageHeader).
        const char* body = data + sbe::MessageHeader::SIZE;
        const char* hmac_sig = body; // first 32 bytes
        const char* hmac_data = body + 32; // everything after signature
        size_t hmac_data_len = len - sbe::MessageHeader::SIZE - 32;

        if (!verifyHmac(hmac_sig, hmac_data, hmac_data_len)) {
            logger_->warn("UUID={}: Negotiate500 HMAC verification failed, terminating", uuid_);
            terminate(8); // HMAC error
            return;
        }
    }

    state_ = SessionState::Negotiated;
    sendNegotiationResponse(neg.sendingTime);
}

void Session::handleEstablish(const char* data, size_t len) {
    if (state_ != SessionState::Negotiated) {
        logger_->warn("UUID={}: Establish503 received in state {}, ignoring",
                      uuid_, sessionStateToString(state_));
        return;
    }

    sbe::Establish503 est;
    est.decode(data, 0);

    logger_->info("UUID={}: Establish503 received (keepAlive={}ms, nextSeqNo={})",
                  uuid_, est.keepAliveInterval, est.nextSeqNo);

    // Verify HMAC if enabled
    if (hmac_enabled_) {
        const char* body = data + sbe::MessageHeader::SIZE;
        const char* hmac_sig = body;
        const char* hmac_data = body + 32;
        size_t hmac_data_len = len - sbe::MessageHeader::SIZE - 32;

        if (!verifyHmac(hmac_sig, hmac_data, hmac_data_len)) {
            logger_->warn("UUID={}: Establish503 HMAC verification failed, terminating", uuid_);
            terminate(8);
            return;
        }
    }

    // Accept the client's keepalive interval (clamped to reasonable range)
    keep_alive_interval_ms_ = est.keepAliveInterval;
    if (keep_alive_interval_ms_ < 1000)  keep_alive_interval_ms_ = 1000;
    if (keep_alive_interval_ms_ > 60000) keep_alive_interval_ms_ = 60000;

    // Accept the client's next expected inbound seq no
    next_in_seq_ = est.nextSeqNo;

    state_ = SessionState::Established;
    last_received_ = std::chrono::steady_clock::now();
    last_sent_ = std::chrono::steady_clock::now();

    sendEstablishmentAck(est.sendingTime, static_cast<uint16_t>(keep_alive_interval_ms_));
}

void Session::handleSequence(const char* data, size_t len) {
    if (state_ != SessionState::Established) {
        logger_->debug("UUID={}: Sequence506 in state {}, ignoring",
                       uuid_, sessionStateToString(state_));
        return;
    }

    sbe::Sequence506 seq;
    seq.decode(data, 0);

    logger_->debug("UUID={}: Sequence506 heartbeat (nextSeqNo={}, lapsed={})",
                   uuid_, seq.nextSeqNo, seq.keepAliveIntervalLapsed);

    // The client's NextSeqNo tells us what they'll send next.
    // If it's ahead of what we expected, there's a gap on the client side --
    // but for heartbeats we simply accept.
    (void)len;
}

void Session::handleTerminate(const char* data, size_t len) {
    (void)len;
    sbe::Terminate507 term;
    term.decode(data, 0);

    logger_->info("UUID={}: Terminate507 received (errorCodes={})", uuid_, term.errorCodes);

    // Respond with our own Terminate
    if (state_ != SessionState::Terminated) {
        sendTerminate(0);
        state_ = SessionState::Terminated;
    }
}

void Session::handleRetransmitRequest(const char* data, size_t len) {
    (void)len;
    if (state_ != SessionState::Established) {
        logger_->warn("UUID={}: RetransmitRequest508 in state {}, ignoring",
                      uuid_, sessionStateToString(state_));
        return;
    }

    sbe::RetransmitRequest508 req;
    req.decode(data, 0);

    logger_->info("UUID={}: RetransmitRequest508 (lastUUID={}, fromSeq={}, count={})",
                  uuid_, req.lastUUID, req.fromSeqNo, req.msgCount);

    // Only retransmit for the current UUID
    if (req.lastUUID != 0 && req.lastUUID != uuid_) {
        logger_->warn("UUID={}: RetransmitRequest for different UUID {}, sending count=0",
                      uuid_, req.lastUUID);
        sendRetransmission(req.lastUUID, req.requestTimestamp, req.fromSeqNo, 0);
        return;
    }

    // Send Retransmission header
    uint16_t actual_count = 0;
    for (uint16_t i = 0; i < req.msgCount; ++i) {
        uint32_t seq = req.fromSeqNo + i;
        size_t entry_len = 0;
        const char* entry_data = retransmit_buffer_.retrieve(seq, entry_len);
        if (entry_data) {
            ++actual_count;
        }
    }

    sendRetransmission(req.lastUUID, req.requestTimestamp, req.fromSeqNo, actual_count);

    // Now retransmit the actual messages
    for (uint16_t i = 0; i < req.msgCount; ++i) {
        uint32_t seq = req.fromSeqNo + i;
        size_t entry_len = 0;
        const char* entry_data = retransmit_buffer_.retrieve(seq, entry_len);
        if (entry_data) {
            sendFramedMessage(entry_data, entry_len);
        }
    }
}

void Session::handleApplicationMessage(uint16_t templateId, const char* data, size_t len) {
    if (state_ != SessionState::Established) {
        logger_->warn("UUID={}: app message (templateId={}) in state {}, ignoring",
                      uuid_, templateId, sessionStateToString(state_));
        return;
    }

    // Extract the client's sequence number from the message body.
    // For iLink3 application messages, seqNum is typically at a fixed offset
    // in the body. However, the offset varies by message type.
    // We use next_in_seq_ to track expected sequence and detect gaps.

    // For NewOrderSingle514: seqNum is at body offset 17
    // For OrderCancelReplaceRequest515: seqNum is at body offset 17
    // For OrderCancelRequest516: seqNum is at body offset 17
    // Read seqNum from the appropriate position based on templateId.
    uint32_t client_seq = 0;
    const char* body = data + sbe::MessageHeader::SIZE;

    switch (templateId) {
        case sbe::NewOrderSingle514::TEMPLATE_ID:
            std::memcpy(&client_seq, body + 17, 4); // seqNum at offset 17
            break;
        case sbe::OrderCancelReplaceRequest515::TEMPLATE_ID:
            std::memcpy(&client_seq, body + 17, 4);
            break;
        case sbe::OrderCancelRequest516::TEMPLATE_ID:
            std::memcpy(&client_seq, body + 17, 4);
            break;
        default:
            // Unknown app message type; try offset 17 as common pattern
            if (len >= sbe::MessageHeader::SIZE + 21) {
                std::memcpy(&client_seq, body + 17, 4);
            }
            break;
    }

    // Gap detection: if client_seq > next_in_seq_, messages were skipped
    if (client_seq > next_in_seq_) {
        uint32_t gap_count = client_seq - next_in_seq_;
        logger_->warn("UUID={}: sequence gap detected: expected={}, got={}, gap={}",
                      uuid_, next_in_seq_, client_seq, gap_count);
        sendNotApplied(next_in_seq_, gap_count);
        next_in_seq_ = client_seq;
    }

    if (client_seq == next_in_seq_) {
        ++next_in_seq_;
    }

    // Forward to the gateway layer
    if (app_cb_) {
        app_cb_(uuid_, templateId, data, len);
    }
}

// ---------------------------------------------------------------------------
// Send helpers
// ---------------------------------------------------------------------------

void Session::sendFramedMessage(const char* sbe_data, size_t sbe_len) {
    // Send raw SBE payload — the transport layer (TcpConnection) adds SOFH framing.
    if (send_cb_) {
        send_cb_(sbe_data, sbe_len);
    }
    last_sent_ = std::chrono::steady_clock::now();
}

void Session::sendNegotiationResponse(uint64_t request_timestamp) {
    sbe::NegotiationResponse501 resp;
    resp.uuid = uuid_;
    resp.requestTimestamp = request_timestamp;
    resp.secretKeySecureIDExpiration = 0;
    resp.faultToleranceIndicator = 0;
    resp.splitMsg = 0;
    resp.previousSeqNo = 0;
    resp.previousUUID = 0;

    char buf[256];
    size_t encoded_len = resp.encode(buf, 0);

    logger_->info("UUID={}: sending NegotiationResponse501", uuid_);
    sendFramedMessage(buf, encoded_len);
}

void Session::sendEstablishmentAck(uint64_t request_timestamp, uint16_t keep_alive_interval) {
    sbe::EstablishmentAck504 ack;
    ack.uuid = uuid_;
    ack.requestTimestamp = request_timestamp;
    ack.keepAliveInterval = keep_alive_interval;
    ack.nextSeqNo = next_out_seq_;
    ack.previousSeqNo = 0;
    ack.previousUUID = 0;

    char buf[256];
    size_t encoded_len = ack.encode(buf, 0);

    logger_->info("UUID={}: sending EstablishmentAck504 (nextSeqNo={}, keepAlive={}ms)",
                  uuid_, next_out_seq_, keep_alive_interval);
    sendFramedMessage(buf, encoded_len);
}

void Session::sendSequenceHeartbeat() {
    sbe::Sequence506 seq;
    seq.uuid = uuid_;
    seq.nextSeqNo = next_out_seq_;
    seq.faultToleranceIndicator = 0;
    seq.keepAliveIntervalLapsed = 0;

    char buf[64];
    size_t encoded_len = seq.encode(buf, 0);

    logger_->debug("UUID={}: sending Sequence506 heartbeat (nextSeqNo={})", uuid_, next_out_seq_);
    sendFramedMessage(buf, encoded_len);
}

void Session::sendTerminate(uint16_t error_code) {
    sbe::Terminate507 term;
    term.uuid = uuid_;
    term.requestTimestamp = nowNanos();
    term.errorCodes = error_code;
    term.splitMsg = 0;

    char buf[64];
    size_t encoded_len = term.encode(buf, 0);

    logger_->info("UUID={}: sending Terminate507 (errorCode={})", uuid_, error_code);
    sendFramedMessage(buf, encoded_len);
}

void Session::sendNotApplied(uint32_t from_seq, uint32_t msg_count) {
    sbe::NotApplied513 na;
    na.uuid = uuid_;
    na.fromSeqNo = from_seq;
    na.msgCount = msg_count;

    char buf[64];
    size_t encoded_len = na.encode(buf, 0);

    logger_->info("UUID={}: sending NotApplied513 (fromSeq={}, count={})",
                  uuid_, from_seq, msg_count);
    sendFramedMessage(buf, encoded_len);
}

void Session::sendRetransmission(uint64_t last_uuid, uint64_t request_timestamp,
                                  uint32_t from_seq, uint16_t msg_count) {
    sbe::Retransmission509 rt;
    rt.uuid = uuid_;
    rt.lastUUID = last_uuid;
    rt.requestTimestamp = request_timestamp;
    rt.fromSeqNo = from_seq;
    rt.msgCount = msg_count;
    rt.splitMsg = 0;

    char buf[64];
    size_t encoded_len = rt.encode(buf, 0);

    logger_->info("UUID={}: sending Retransmission509 (fromSeq={}, count={})",
                  uuid_, from_seq, msg_count);
    sendFramedMessage(buf, encoded_len);
}

// ---------------------------------------------------------------------------
// HMAC
// ---------------------------------------------------------------------------

bool Session::verifyHmac(const char* hmac_signature, const char* data, size_t len) {
    if (!hmac_enabled_) {
        return true;
    }
    return HmacAuthenticator::verify(hmac_key_, data, len, hmac_signature);
}

// ---------------------------------------------------------------------------
// Framing
// ---------------------------------------------------------------------------

std::vector<char> Session::frameMessage(const char* sbe_data, size_t sbe_len) {
    uint32_t total_len = static_cast<uint32_t>(sbe::SOFH::SIZE + sbe_len);
    std::vector<char> framed(total_len);
    sbe::SOFH::encode(framed.data(), total_len);
    std::memcpy(framed.data() + sbe::SOFH::SIZE, sbe_data, sbe_len);
    return framed;
}

uint64_t Session::nowNanos() {
    auto now = std::chrono::system_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    return static_cast<uint64_t>(ns);
}

} // namespace cme::sim::fixp
