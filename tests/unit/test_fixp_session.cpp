#include <gtest/gtest.h>
#include "fixp/session.h"
#include "sbe/ilink3_messages.h"
#include "sbe/framing.h"
#include "sbe/message_header.h"
#include <vector>
#include <cstring>

using namespace cme::sim;
using namespace cme::sim::fixp;
using namespace cme::sim::sbe;

class FixpSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        sent_data_.clear();

        auto send_cb = [this](const char* data, size_t len) {
            sent_data_.emplace_back(data, data + len);
        };

        auto app_cb = [this](uint64_t uuid, uint16_t templateId,
                             const char* data, size_t len) {
            app_messages_.push_back({uuid, templateId,
                                     std::vector<char>(data, data + len)});
        };

        session_ = std::make_unique<Session>(test_uuid_, send_cb, app_cb);
        // Disable HMAC so tests don't need valid signatures
        session_->setHmacEnabled(false);
    }

    // Encode a Negotiate500 message (SBE payload without SOFH)
    std::vector<char> encodeNegotiate() {
        Negotiate500 neg;
        neg.uuid = test_uuid_;
        neg.sendingTime = 1234567890;
        writeFixedString(neg.session, "TST", 3);
        writeFixedString(neg.firm, "FIRM1", 5);

        std::vector<char> buf(neg.encodedLength());
        neg.encode(buf.data(), 0);
        return buf;
    }

    // Encode an Establish503 message
    std::vector<char> encodeEstablish(uint16_t keepAlive = 30000, uint32_t nextSeq = 1) {
        Establish503 est;
        est.uuid = test_uuid_;
        est.sendingTime = 2345678901ULL;
        writeFixedString(est.session, "TST", 3);
        writeFixedString(est.firm, "FIRM1", 5);
        est.keepAliveInterval = keepAlive;
        est.nextSeqNo = nextSeq;

        std::vector<char> buf(est.encodedLength());
        est.encode(buf.data(), 0);
        return buf;
    }

    // Encode a Sequence506 (heartbeat)
    std::vector<char> encodeSequence(uint32_t nextSeq = 1) {
        Sequence506 seq;
        seq.uuid = test_uuid_;
        seq.nextSeqNo = nextSeq;
        seq.faultToleranceIndicator = 0;
        seq.keepAliveIntervalLapsed = 0;

        std::vector<char> buf(seq.encodedLength());
        seq.encode(buf.data(), 0);
        return buf;
    }

    // Encode a Terminate507
    std::vector<char> encodeTerminate(uint16_t errorCode = 0) {
        Terminate507 term;
        term.uuid = test_uuid_;
        term.requestTimestamp = 3456789012ULL;
        term.errorCodes = errorCode;

        std::vector<char> buf(term.encodedLength());
        term.encode(buf.data(), 0);
        return buf;
    }

    // Encode a NewOrderSingle514
    std::vector<char> encodeNewOrderSingle(uint32_t seqNum = 1) {
        NewOrderSingle514 nos;
        nos.price = Price::fromDouble(100.0).mantissa;
        nos.orderQty = 10;
        nos.securityID = 1;
        nos.side = static_cast<uint8_t>(Side::Buy);
        nos.seqNum = seqNum;
        writeFixedString(nos.senderID, "SENDER", 20);
        writeFixedString(nos.clOrdID, "CLO001", 20);
        nos.sendingTimeEpoch = 1000000000ULL;
        nos.ordType = static_cast<uint8_t>(OrderType::Limit);
        nos.timeInForce = static_cast<uint8_t>(TimeInForce::Day);

        std::vector<char> buf(nos.encodedLength());
        nos.encode(buf.data(), 0);
        return buf;
    }

    // Get the templateId from a raw SBE response (no SOFH — framing is added by TcpConnection)
    uint16_t getResponseTemplateId(int index) {
        if (index >= static_cast<int>(sent_data_.size())) return 0;
        const auto& frame = sent_data_[index];
        if (frame.size() < MessageHeader::SIZE) return 0;
        return MessageHeader::decodeTemplateId(frame.data());
    }

    // Negotiate the session (from Connected -> Negotiated)
    void doNegotiate() {
        auto neg = encodeNegotiate();
        session_->onMessage(neg.data(), neg.size());
    }

    // Establish the session (from Negotiated -> Established)
    void doEstablish() {
        auto est = encodeEstablish();
        session_->onMessage(est.data(), est.size());
    }

    static constexpr uint64_t test_uuid_ = 42;
    std::unique_ptr<Session> session_;
    std::vector<std::vector<char>> sent_data_;

    struct AppMsg {
        uint64_t uuid;
        uint16_t templateId;
        std::vector<char> data;
    };
    std::vector<AppMsg> app_messages_;
};

// ---------------------------------------------------------------------------
// 1. NegotiateTransition
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, NegotiateTransition) {
    ASSERT_EQ(session_->state(), SessionState::Connected);

    doNegotiate();

    EXPECT_EQ(session_->state(), SessionState::Negotiated);
    // Should have sent NegotiationResponse501
    ASSERT_GE(sent_data_.size(), 1u);
    EXPECT_EQ(getResponseTemplateId(0), NegotiationResponse501::TEMPLATE_ID);
}

// ---------------------------------------------------------------------------
// 2. EstablishTransition
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, EstablishTransition) {
    doNegotiate();
    ASSERT_EQ(session_->state(), SessionState::Negotiated);

    doEstablish();

    EXPECT_EQ(session_->state(), SessionState::Established);
    // Should have sent EstablishmentAck504
    ASSERT_GE(sent_data_.size(), 2u);
    EXPECT_EQ(getResponseTemplateId(1), EstablishmentAck504::TEMPLATE_ID);
}

// ---------------------------------------------------------------------------
// 3. HeartbeatHandling
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, HeartbeatHandling) {
    doNegotiate();
    doEstablish();
    ASSERT_EQ(session_->state(), SessionState::Established);

    size_t sent_before = sent_data_.size();
    auto seq = encodeSequence(1);
    session_->onMessage(seq.data(), seq.size());

    // State should remain Established
    EXPECT_EQ(session_->state(), SessionState::Established);
    // Sequence heartbeat doesn't generate a response
    EXPECT_EQ(sent_data_.size(), sent_before);
}

// ---------------------------------------------------------------------------
// 4. TerminateHandling
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, TerminateHandling) {
    doNegotiate();
    doEstablish();
    ASSERT_EQ(session_->state(), SessionState::Established);

    auto term = encodeTerminate();
    session_->onMessage(term.data(), term.size());

    EXPECT_EQ(session_->state(), SessionState::Terminated);
    // Should have sent a Terminate507 in response
    bool found_terminate = false;
    for (size_t i = 0; i < sent_data_.size(); ++i) {
        if (getResponseTemplateId(static_cast<int>(i)) == Terminate507::TEMPLATE_ID) {
            found_terminate = true;
        }
    }
    EXPECT_TRUE(found_terminate);
}

// ---------------------------------------------------------------------------
// 5. InvalidStateNegotiate
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, InvalidStateNegotiate) {
    // Move to Established first
    doNegotiate();
    doEstablish();
    ASSERT_EQ(session_->state(), SessionState::Established);

    size_t sent_before = sent_data_.size();

    // Sending Negotiate in Established state should be ignored
    auto neg = encodeNegotiate();
    session_->onMessage(neg.data(), neg.size());

    // State unchanged
    EXPECT_EQ(session_->state(), SessionState::Established);
    // No NegotiationResponse should be sent
    EXPECT_EQ(sent_data_.size(), sent_before);
}

// ---------------------------------------------------------------------------
// 6. InvalidStateEstablish
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, InvalidStateEstablish) {
    // Connected state -> Establish should be ignored
    ASSERT_EQ(session_->state(), SessionState::Connected);

    auto est = encodeEstablish();
    session_->onMessage(est.data(), est.size());

    EXPECT_EQ(session_->state(), SessionState::Connected);
    EXPECT_EQ(sent_data_.size(), 0u);
}

// ---------------------------------------------------------------------------
// 7. SequenceTracking
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, SequenceTracking) {
    doNegotiate();
    doEstablish();

    // Initial outbound seq should be 1
    EXPECT_EQ(session_->nextOutSeqNo(), 1u);

    // Sending an app message should increment seq
    char dummy_sbe[64];
    // Encode a simple Sequence506 as dummy app data
    Sequence506 dummy;
    dummy.uuid = test_uuid_;
    dummy.nextSeqNo = 1;
    size_t len = dummy.encode(dummy_sbe, 0);

    session_->sendApplicationMessage(dummy_sbe, len);
    EXPECT_EQ(session_->nextOutSeqNo(), 2u);

    session_->sendApplicationMessage(dummy_sbe, len);
    EXPECT_EQ(session_->nextOutSeqNo(), 3u);
}

// ---------------------------------------------------------------------------
// 8. FullLifecycle
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, FullLifecycle) {
    // Connected -> Negotiate
    EXPECT_EQ(session_->state(), SessionState::Connected);
    doNegotiate();
    EXPECT_EQ(session_->state(), SessionState::Negotiated);

    // Negotiated -> Establish
    doEstablish();
    EXPECT_EQ(session_->state(), SessionState::Established);

    // Send an application message (NOS)
    auto nos = encodeNewOrderSingle(1);
    session_->onMessage(nos.data(), nos.size());
    ASSERT_EQ(app_messages_.size(), 1u);
    EXPECT_EQ(app_messages_[0].templateId, NewOrderSingle514::TEMPLATE_ID);

    // Send heartbeat
    auto seq = encodeSequence(2);
    session_->onMessage(seq.data(), seq.size());
    EXPECT_EQ(session_->state(), SessionState::Established);

    // Terminate
    auto term = encodeTerminate();
    session_->onMessage(term.data(), term.size());
    EXPECT_EQ(session_->state(), SessionState::Terminated);
}

// ---------------------------------------------------------------------------
// 9. MessageTooSmall
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, MessageTooSmall) {
    char tiny[2] = {0, 0};
    session_->onMessage(tiny, 2);
    // Should be silently ignored
    EXPECT_EQ(session_->state(), SessionState::Connected);
    EXPECT_EQ(sent_data_.size(), 0u);
}

// ---------------------------------------------------------------------------
// 10. TerminateFromServerSide
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, TerminateFromServerSide) {
    doNegotiate();
    doEstablish();

    session_->terminate(42);
    EXPECT_EQ(session_->state(), SessionState::Terminated);

    // Should have sent a Terminate507
    bool found = false;
    for (size_t i = 0; i < sent_data_.size(); ++i) {
        if (getResponseTemplateId(static_cast<int>(i)) == Terminate507::TEMPLATE_ID) {
            found = true;
        }
    }
    EXPECT_TRUE(found);

    // Double terminate should be a no-op
    size_t sent_before = sent_data_.size();
    session_->terminate(0);
    EXPECT_EQ(sent_data_.size(), sent_before);
}

// ---------------------------------------------------------------------------
// 11. AppMessageInWrongState
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, AppMessageInWrongState) {
    // NOS in Connected state should be ignored
    auto nos = encodeNewOrderSingle(1);
    session_->onMessage(nos.data(), nos.size());
    EXPECT_EQ(app_messages_.size(), 0u);
}

// ---------------------------------------------------------------------------
// 12. SendAppMessageInWrongState
// ---------------------------------------------------------------------------
TEST_F(FixpSessionTest, SendAppMessageInWrongState) {
    // sendApplicationMessage before Established should be ignored
    char dummy[16] = {};
    size_t sent_before = sent_data_.size();
    session_->sendApplicationMessage(dummy, 16);
    EXPECT_EQ(sent_data_.size(), sent_before);
}
