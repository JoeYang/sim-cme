#include <gtest/gtest.h>
#include "fixp/session.h"
#include "sbe/ilink3_messages.h"
#include "sbe/framing.h"
#include "sbe/message_header.h"
#include "common/types.h"
#include <vector>
#include <cstring>

using namespace cme::sim;
using namespace cme::sim::fixp;
using namespace cme::sim::sbe;

// Full integration test: create Session, simulate TCP-like message exchange
// for the complete FIXP lifecycle: Negotiate -> Establish -> App Messages -> Terminate.

class FixpLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        sent_frames_.clear();
        app_messages_.clear();

        auto send_cb = [this](const char* data, size_t len) {
            sent_frames_.emplace_back(data, data + len);
        };

        auto app_cb = [this](uint64_t uuid, uint16_t templateId,
                             const char* data, size_t len) {
            app_messages_.push_back({uuid, templateId,
                                     std::vector<char>(data, data + len)});
        };

        session_ = std::make_unique<Session>(test_uuid_, send_cb, app_cb);
        session_->setHmacEnabled(false);
    }

    // Decode a response from the sent_frames_ at given index (raw SBE, no SOFH)
    uint16_t responseTemplateId(int idx) {
        if (idx >= static_cast<int>(sent_frames_.size())) return 0;
        const auto& frame = sent_frames_[idx];
        if (frame.size() < MessageHeader::SIZE) return 0;
        return MessageHeader::decodeTemplateId(frame.data());
    }

    // Decode NegotiationResponse501 from a raw SBE response
    NegotiationResponse501 decodeNegotiationResponse(int idx) {
        NegotiationResponse501 resp;
        const auto& frame = sent_frames_[idx];
        resp.decode(frame.data(), 0);
        return resp;
    }

    // Decode EstablishmentAck504 from a raw SBE response
    EstablishmentAck504 decodeEstablishmentAck(int idx) {
        EstablishmentAck504 ack;
        const auto& frame = sent_frames_[idx];
        ack.decode(frame.data(), 0);
        return ack;
    }

    static constexpr uint64_t test_uuid_ = 12345;
    std::unique_ptr<Session> session_;
    std::vector<std::vector<char>> sent_frames_;

    struct AppMsg {
        uint64_t uuid;
        uint16_t templateId;
        std::vector<char> data;
    };
    std::vector<AppMsg> app_messages_;
};

// ---------------------------------------------------------------------------
// Full Lifecycle: Negotiate -> Establish -> NOS x3 -> Heartbeat -> Terminate
// ---------------------------------------------------------------------------
TEST_F(FixpLifecycleTest, CompleteLifecycle) {
    // ========== Phase 1: Negotiate ==========
    EXPECT_EQ(session_->state(), SessionState::Connected);

    Negotiate500 neg;
    neg.uuid = test_uuid_;
    neg.sendingTime = 1000000000ULL;
    writeFixedString(neg.session, "TST", 3);
    writeFixedString(neg.firm, "FIRM1", 5);

    char neg_buf[256];
    size_t neg_len = neg.encode(neg_buf, 0);
    session_->onMessage(neg_buf, neg_len);

    EXPECT_EQ(session_->state(), SessionState::Negotiated);
    ASSERT_EQ(sent_frames_.size(), 1u);
    EXPECT_EQ(responseTemplateId(0), NegotiationResponse501::TEMPLATE_ID);

    // Verify the NegotiationResponse contains our UUID
    auto neg_resp = decodeNegotiationResponse(0);
    EXPECT_EQ(neg_resp.uuid, test_uuid_);
    EXPECT_EQ(neg_resp.requestTimestamp, 1000000000ULL);

    // ========== Phase 2: Establish ==========
    Establish503 est;
    est.uuid = test_uuid_;
    est.sendingTime = 2000000000ULL;
    writeFixedString(est.session, "TST", 3);
    writeFixedString(est.firm, "FIRM1", 5);
    est.keepAliveInterval = 30000;
    est.nextSeqNo = 1;

    char est_buf[256];
    size_t est_len = est.encode(est_buf, 0);
    session_->onMessage(est_buf, est_len);

    EXPECT_EQ(session_->state(), SessionState::Established);
    ASSERT_EQ(sent_frames_.size(), 2u);
    EXPECT_EQ(responseTemplateId(1), EstablishmentAck504::TEMPLATE_ID);

    auto est_ack = decodeEstablishmentAck(1);
    EXPECT_EQ(est_ack.uuid, test_uuid_);
    EXPECT_EQ(est_ack.nextSeqNo, 1u); // server's next outbound seq
    EXPECT_EQ(est_ack.keepAliveInterval, 30000u);

    // ========== Phase 3: Application Messages ==========
    // Send 3 NewOrderSingle514 messages
    for (uint32_t seq = 1; seq <= 3; ++seq) {
        NewOrderSingle514 nos;
        nos.price = Price::fromDouble(5000.0 + seq * 0.25).mantissa;
        nos.orderQty = seq * 10;
        nos.securityID = 12345;
        nos.side = static_cast<uint8_t>(Side::Buy);
        nos.seqNum = seq;
        writeFixedString(nos.senderID, "SENDER01", 20);
        char clord[21];
        snprintf(clord, sizeof(clord), "CLO%03u", seq);
        writeFixedString(nos.clOrdID, clord, 20);
        nos.ordType = static_cast<uint8_t>(OrderType::Limit);
        nos.timeInForce = static_cast<uint8_t>(TimeInForce::Day);
        nos.sendingTimeEpoch = 3000000000ULL + seq;

        char nos_buf[256];
        size_t nos_len = nos.encode(nos_buf, 0);
        session_->onMessage(nos_buf, nos_len);
    }

    // All 3 should have been forwarded to app callback
    ASSERT_EQ(app_messages_.size(), 3u);
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(app_messages_[i].uuid, test_uuid_);
        EXPECT_EQ(app_messages_[i].templateId, NewOrderSingle514::TEMPLATE_ID);
    }

    // ========== Phase 4: Heartbeat ==========
    Sequence506 hb;
    hb.uuid = test_uuid_;
    hb.nextSeqNo = 4; // client's next seq
    hb.keepAliveIntervalLapsed = 0;

    char hb_buf[64];
    size_t hb_len = hb.encode(hb_buf, 0);
    session_->onMessage(hb_buf, hb_len);

    EXPECT_EQ(session_->state(), SessionState::Established);

    // ========== Phase 5: Terminate ==========
    Terminate507 term;
    term.uuid = test_uuid_;
    term.requestTimestamp = 9000000000ULL;
    term.errorCodes = 0;

    char term_buf[64];
    size_t term_len = term.encode(term_buf, 0);
    session_->onMessage(term_buf, term_len);

    EXPECT_EQ(session_->state(), SessionState::Terminated);

    // Should have sent a Terminate507 response
    bool found_terminate = false;
    for (size_t i = 0; i < sent_frames_.size(); ++i) {
        if (responseTemplateId(static_cast<int>(i)) == Terminate507::TEMPLATE_ID) {
            found_terminate = true;
        }
    }
    EXPECT_TRUE(found_terminate);
}

// ---------------------------------------------------------------------------
// Test: Multiple sessions can coexist
// ---------------------------------------------------------------------------
TEST_F(FixpLifecycleTest, MultipleSessions) {
    // Create a second session with different UUID
    std::vector<std::vector<char>> sent2;
    std::vector<AppMsg> app2;

    auto send_cb2 = [&sent2](const char* data, size_t len) {
        sent2.emplace_back(data, data + len);
    };
    auto app_cb2 = [&app2](uint64_t uuid, uint16_t templateId,
                           const char* data, size_t len) {
        app2.push_back({uuid, templateId, std::vector<char>(data, data + len)});
    };

    Session session2(99999, send_cb2, app_cb2);
    session2.setHmacEnabled(false);

    // Negotiate session 1
    {
        Negotiate500 neg;
        neg.uuid = test_uuid_;
        neg.sendingTime = 1000ULL;
        char buf[256];
        size_t len = neg.encode(buf, 0);
        session_->onMessage(buf, len);
    }
    EXPECT_EQ(session_->state(), SessionState::Negotiated);

    // Negotiate session 2
    {
        Negotiate500 neg;
        neg.uuid = 99999;
        neg.sendingTime = 2000ULL;
        char buf[256];
        size_t len = neg.encode(buf, 0);
        session2.onMessage(buf, len);
    }
    EXPECT_EQ(session2.state(), SessionState::Negotiated);

    // They should be independent
    EXPECT_EQ(session_->uuid(), test_uuid_);
    EXPECT_EQ(session2.uuid(), 99999u);
}

// ---------------------------------------------------------------------------
// Test: Server-initiated terminate
// ---------------------------------------------------------------------------
TEST_F(FixpLifecycleTest, ServerTerminate) {
    // Negotiate + Establish
    {
        Negotiate500 neg;
        neg.uuid = test_uuid_;
        neg.sendingTime = 1000ULL;
        char buf[256];
        size_t len = neg.encode(buf, 0);
        session_->onMessage(buf, len);
    }
    {
        Establish503 est;
        est.uuid = test_uuid_;
        est.sendingTime = 2000ULL;
        est.keepAliveInterval = 30000;
        est.nextSeqNo = 1;
        char buf[256];
        size_t len = est.encode(buf, 0);
        session_->onMessage(buf, len);
    }
    ASSERT_EQ(session_->state(), SessionState::Established);

    // Server terminates the session
    session_->terminate(42);
    EXPECT_EQ(session_->state(), SessionState::Terminated);

    // Verify Terminate507 was sent
    bool found = false;
    for (size_t i = 0; i < sent_frames_.size(); ++i) {
        if (responseTemplateId(static_cast<int>(i)) == Terminate507::TEMPLATE_ID) {
            // Decode and check error code
            Terminate507 term;
            term.decode(sent_frames_[i].data(), 0);
            if (term.errorCodes == 42) {
                found = true;
            }
        }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// Test: Retransmit request and response
// ---------------------------------------------------------------------------
TEST_F(FixpLifecycleTest, RetransmitRequestResponse) {
    // Negotiate + Establish
    {
        Negotiate500 neg;
        neg.uuid = test_uuid_;
        neg.sendingTime = 1000ULL;
        char buf[256];
        neg.encode(buf, 0);
        session_->onMessage(buf, neg.encodedLength());
    }
    {
        Establish503 est;
        est.uuid = test_uuid_;
        est.sendingTime = 2000ULL;
        est.keepAliveInterval = 30000;
        est.nextSeqNo = 1;
        char buf[256];
        est.encode(buf, 0);
        session_->onMessage(buf, est.encodedLength());
    }
    ASSERT_EQ(session_->state(), SessionState::Established);

    // Send some application messages from the server side
    // (these get stored in the retransmit buffer)
    for (int i = 0; i < 3; ++i) {
        ExecutionReportNew522 er;
        er.seqNum = i + 1;
        er.uuid = test_uuid_;
        er.orderID = 100 + i;
        char buf[512];
        size_t len = er.encode(buf, 0);
        session_->sendApplicationMessage(buf, len);
    }

    EXPECT_EQ(session_->nextOutSeqNo(), 4u); // 3 messages sent

    size_t sent_before = sent_frames_.size();

    // Client sends RetransmitRequest for seq 1-3
    RetransmitRequest508 req;
    req.uuid = test_uuid_;
    req.lastUUID = test_uuid_;
    req.requestTimestamp = 5000ULL;
    req.fromSeqNo = 1;
    req.msgCount = 3;

    char req_buf[128];
    req.encode(req_buf, 0);
    session_->onMessage(req_buf, req.encodedLength());

    // Should have sent: 1 Retransmission509 header + 3 retransmitted messages
    size_t new_frames = sent_frames_.size() - sent_before;
    EXPECT_EQ(new_frames, 4u);

    // First should be Retransmission509
    EXPECT_EQ(responseTemplateId(static_cast<int>(sent_before)),
              Retransmission509::TEMPLATE_ID);
}
