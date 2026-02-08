#include <gtest/gtest.h>
#include "sbe/framing.h"
#include "sbe/message_header.h"
#include "sbe/ilink3_messages.h"
#include "sbe/mdp3_messages.h"
#include "common/types.h"
#include <cstring>
#include <vector>

using namespace cme::sim::sbe;

// ===========================================================================
// SOFH Tests
// ===========================================================================

TEST(SBECodec, SOFHRoundtrip) {
    char buf[SOFH::SIZE];
    uint32_t msg_len = 42;
    SOFH::encode(buf, msg_len);

    EXPECT_EQ(SOFH::decodeMessageLength(buf), 42u);
    EXPECT_EQ(SOFH::decodeEncodingType(buf), SOFH::SBE_ENCODING_TYPE);
    EXPECT_TRUE(SOFH::isValidSBE(buf));
}

TEST(SBECodec, SOFHFramedLength) {
    EXPECT_EQ(SOFH::framedLength(100), 106u); // 6 + 100
}

// ===========================================================================
// MessageHeader Tests
// ===========================================================================

TEST(SBECodec, MessageHeaderRoundtrip) {
    char buf[MessageHeader::SIZE];
    uint16_t blockLen = 80;
    uint16_t templateId = 514;
    uint16_t schemaId = 8;
    uint16_t version = 8;

    MessageHeader::encode(buf, blockLen, templateId, schemaId, version);

    EXPECT_EQ(MessageHeader::decodeBlockLength(buf), 80u);
    EXPECT_EQ(MessageHeader::decodeTemplateId(buf), 514u);
    EXPECT_EQ(MessageHeader::decodeSchemaId(buf), 8u);
    EXPECT_EQ(MessageHeader::decodeVersion(buf), 8u);
}

TEST(SBECodec, MessageHeaderILink3) {
    char buf[MessageHeader::SIZE];
    MessageHeader::encodeILink3(buf, 100, 500);

    EXPECT_EQ(MessageHeader::decodeBlockLength(buf), 100u);
    EXPECT_EQ(MessageHeader::decodeTemplateId(buf), 500u);
    EXPECT_EQ(MessageHeader::decodeSchemaId(buf), MessageHeader::ILINK3_SCHEMA_ID);
    EXPECT_EQ(MessageHeader::decodeVersion(buf), MessageHeader::ILINK3_VERSION);
}

TEST(SBECodec, MessageHeaderMDP3) {
    char buf[MessageHeader::SIZE];
    MessageHeader::encodeMDP3(buf, 9, 46);

    EXPECT_EQ(MessageHeader::decodeBlockLength(buf), 9u);
    EXPECT_EQ(MessageHeader::decodeTemplateId(buf), 46u);
    EXPECT_EQ(MessageHeader::decodeSchemaId(buf), MessageHeader::MDP3_SCHEMA_ID);
    EXPECT_EQ(MessageHeader::decodeVersion(buf), MessageHeader::MDP3_VERSION);
}

// ===========================================================================
// iLink3 Message Roundtrip Tests
// ===========================================================================

TEST(SBECodec, Negotiate500Roundtrip) {
    Negotiate500 orig;
    std::memset(orig.hmacSignature, 0xAB, 32);
    writeFixedString(orig.accessKeyID, "MYACCESSKEY12345", 20);
    orig.uuid = 123456789ULL;
    orig.sendingTime = 9876543210ULL;
    writeFixedString(orig.session, "AB", 3);
    writeFixedString(orig.firm, "FRMX", 5);
    orig.cancelOnDisconnectIndicator = 1;
    orig.cancelOnRejectConID = 55ULL;

    char buf[256];
    size_t len = orig.encode(buf, 0);
    EXPECT_EQ(len, orig.encodedLength());

    Negotiate500 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(std::memcmp(decoded.hmacSignature, orig.hmacSignature, 32), 0);
    EXPECT_EQ(decoded.uuid, 123456789ULL);
    EXPECT_EQ(decoded.sendingTime, 9876543210ULL);
    EXPECT_EQ(decoded.cancelOnDisconnectIndicator, 1u);
    EXPECT_EQ(decoded.cancelOnRejectConID, 55ULL);
}

TEST(SBECodec, NegotiationResponse501Roundtrip) {
    NegotiationResponse501 orig;
    orig.uuid = 42;
    orig.requestTimestamp = 100200300ULL;
    orig.secretKeySecureIDExpiration = 7;
    orig.faultToleranceIndicator = 1;
    orig.splitMsg = 2;
    orig.previousSeqNo = 10;
    orig.previousUUID = 99;

    char buf[256];
    size_t len = orig.encode(buf, 0);
    EXPECT_EQ(len, orig.encodedLength());

    NegotiationResponse501 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.uuid, 42u);
    EXPECT_EQ(decoded.requestTimestamp, 100200300ULL);
    EXPECT_EQ(decoded.secretKeySecureIDExpiration, 7u);
    EXPECT_EQ(decoded.faultToleranceIndicator, 1u);
    EXPECT_EQ(decoded.splitMsg, 2u);
    EXPECT_EQ(decoded.previousSeqNo, 10u);
    EXPECT_EQ(decoded.previousUUID, 99u);
}

TEST(SBECodec, Establish503Roundtrip) {
    Establish503 orig;
    std::memset(orig.hmacSignature, 0xCD, 32);
    writeFixedString(orig.accessKeyID, "KEY123", 20);
    orig.uuid = 555;
    orig.sendingTime = 777888999ULL;
    writeFixedString(orig.session, "XY", 3);
    writeFixedString(orig.firm, "FIR", 5);
    orig.keepAliveInterval = 30000;
    orig.nextSeqNo = 5;
    orig.cancelOnDisconnectIndicator = 0;

    char buf[256];
    size_t len = orig.encode(buf, 0);
    EXPECT_EQ(len, orig.encodedLength());

    Establish503 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.uuid, 555u);
    EXPECT_EQ(decoded.sendingTime, 777888999ULL);
    EXPECT_EQ(decoded.keepAliveInterval, 30000u);
    EXPECT_EQ(decoded.nextSeqNo, 5u);
}

TEST(SBECodec, EstablishmentAck504Roundtrip) {
    EstablishmentAck504 orig;
    orig.uuid = 42;
    orig.requestTimestamp = 111222333ULL;
    orig.keepAliveInterval = 10000;
    orig.nextSeqNo = 1;
    orig.previousSeqNo = 0;
    orig.previousUUID = 0;

    char buf[256];
    size_t len = orig.encode(buf, 0);

    EstablishmentAck504 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.uuid, 42u);
    EXPECT_EQ(decoded.requestTimestamp, 111222333ULL);
    EXPECT_EQ(decoded.keepAliveInterval, 10000u);
    EXPECT_EQ(decoded.nextSeqNo, 1u);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, Sequence506Roundtrip) {
    Sequence506 orig;
    orig.uuid = 42;
    orig.nextSeqNo = 100;
    orig.faultToleranceIndicator = 1;
    orig.keepAliveIntervalLapsed = 1;

    char buf[64];
    size_t len = orig.encode(buf, 0);

    Sequence506 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.uuid, 42u);
    EXPECT_EQ(decoded.nextSeqNo, 100u);
    EXPECT_EQ(decoded.faultToleranceIndicator, 1u);
    EXPECT_EQ(decoded.keepAliveIntervalLapsed, 1u);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, Terminate507Roundtrip) {
    Terminate507 orig;
    orig.uuid = 42;
    orig.requestTimestamp = 999888777ULL;
    orig.errorCodes = 5;
    orig.splitMsg = 0;

    char buf[64];
    size_t len = orig.encode(buf, 0);

    Terminate507 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.uuid, 42u);
    EXPECT_EQ(decoded.requestTimestamp, 999888777ULL);
    EXPECT_EQ(decoded.errorCodes, 5u);
    EXPECT_EQ(decoded.splitMsg, 0u);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, NewOrderSingle514Roundtrip) {
    NewOrderSingle514 orig;
    orig.price = static_cast<int64_t>(5000.25 * 1e9); // fixed-point
    orig.orderQty = 10;
    orig.securityID = 12345;
    orig.side = 1; // Buy
    orig.seqNum = 42;
    writeFixedString(orig.senderID, "SENDER01", 20);
    writeFixedString(orig.clOrdID, "CLORD001", 20);
    orig.partyDetailsListReqID = 999;
    orig.orderRequestID = 888;
    orig.sendingTimeEpoch = 1000000000ULL;
    orig.stopPx = 0;
    writeFixedString(orig.location, "US", 5);
    orig.minQty = 0;
    orig.displayQty = 10;
    orig.expireDate = 20251231;
    orig.ordType = 2; // Limit
    orig.timeInForce = 0; // Day
    orig.manualOrderIndicator = 0;

    char buf[512];
    size_t len = orig.encode(buf, 0);
    EXPECT_EQ(len, orig.encodedLength());

    NewOrderSingle514 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.price, orig.price);
    EXPECT_EQ(decoded.orderQty, 10u);
    EXPECT_EQ(decoded.securityID, 12345);
    EXPECT_EQ(decoded.side, 1u);
    EXPECT_EQ(decoded.seqNum, 42u);
    EXPECT_EQ(decoded.partyDetailsListReqID, 999u);
    EXPECT_EQ(decoded.orderRequestID, 888u);
    EXPECT_EQ(decoded.sendingTimeEpoch, 1000000000ULL);
    EXPECT_EQ(decoded.ordType, 2u);
    EXPECT_EQ(decoded.timeInForce, 0u);

    // Verify string fields
    char sender_buf[21]{};
    readFixedString(sender_buf, decoded.senderID, 20);
    EXPECT_STREQ(sender_buf, "SENDER01");
}

TEST(SBECodec, ExecutionReportNew522Roundtrip) {
    ExecutionReportNew522 orig;
    orig.seqNum = 1;
    orig.uuid = 42;
    writeFixedString(orig.execID, "EXEC001", 40);
    writeFixedString(orig.senderID, "SND", 20);
    writeFixedString(orig.clOrdID, "CLO001", 20);
    orig.partyDetailsListReqID = 100;
    orig.orderID = 200;
    orig.price = static_cast<int64_t>(100.0 * 1e9);
    orig.transactTime = 5000000000ULL;
    orig.sendingTimeEpoch = 6000000000ULL;
    orig.orderRequestID = 300;
    orig.securityID = 12345;
    orig.orderQty = 10;
    orig.ordType = 2;
    orig.side = 1;
    orig.timeInForce = 0;

    char buf[512];
    size_t len = orig.encode(buf, 0);

    ExecutionReportNew522 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.seqNum, 1u);
    EXPECT_EQ(decoded.uuid, 42u);
    EXPECT_EQ(decoded.orderID, 200u);
    EXPECT_EQ(decoded.price, orig.price);
    EXPECT_EQ(decoded.securityID, 12345);
    EXPECT_EQ(decoded.orderQty, 10u);
    EXPECT_EQ(decoded.ordType, 2u);
    EXPECT_EQ(decoded.side, 1u);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, ExecutionReportTradeOutright525Roundtrip) {
    ExecutionReportTradeOutright525 orig;
    orig.seqNum = 5;
    orig.uuid = 42;
    writeFixedString(orig.execID, "TRADE001", 40);
    writeFixedString(orig.senderID, "SND", 20);
    writeFixedString(orig.clOrdID, "CLO001", 20);
    orig.orderID = 1000;
    orig.price = static_cast<int64_t>(5000.0 * 1e9);
    orig.transactTime = 7000000000ULL;
    orig.lastQty = 5;
    orig.lastPx = static_cast<int64_t>(5000.0 * 1e9);
    orig.securityID = 12345;
    orig.orderQty = 10;
    orig.cumQty = 5;
    orig.leavesQty = 5;
    orig.sideTradeID = 42;
    orig.ordType = 2;
    orig.side = 1;
    orig.ordStatus = '1'; // PartiallyFilled
    orig.execType = 'F';  // Trade
    orig.aggressorIndicator = 1;
    orig.fillPx = orig.lastPx;
    orig.fillQty = 5;

    char buf[512];
    size_t len = orig.encode(buf, 0);

    ExecutionReportTradeOutright525 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.seqNum, 5u);
    EXPECT_EQ(decoded.uuid, 42u);
    EXPECT_EQ(decoded.orderID, 1000u);
    EXPECT_EQ(decoded.lastQty, 5u);
    EXPECT_EQ(decoded.lastPx, orig.lastPx);
    EXPECT_EQ(decoded.cumQty, 5u);
    EXPECT_EQ(decoded.leavesQty, 5u);
    EXPECT_EQ(decoded.ordStatus, '1');
    EXPECT_EQ(decoded.execType, 'F');
    EXPECT_EQ(decoded.aggressorIndicator, 1u);
    EXPECT_EQ(decoded.fillPx, orig.fillPx);
    EXPECT_EQ(decoded.fillQty, 5u);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, RetransmitRequest508Roundtrip) {
    RetransmitRequest508 orig;
    orig.uuid = 42;
    orig.lastUUID = 41;
    orig.requestTimestamp = 999ULL;
    orig.fromSeqNo = 10;
    orig.msgCount = 5;

    char buf[128];
    size_t len = orig.encode(buf, 0);

    RetransmitRequest508 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.uuid, 42u);
    EXPECT_EQ(decoded.lastUUID, 41u);
    EXPECT_EQ(decoded.requestTimestamp, 999u);
    EXPECT_EQ(decoded.fromSeqNo, 10u);
    EXPECT_EQ(decoded.msgCount, 5u);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, Retransmission509Roundtrip) {
    Retransmission509 orig;
    orig.uuid = 42;
    orig.lastUUID = 41;
    orig.requestTimestamp = 888ULL;
    orig.fromSeqNo = 10;
    orig.msgCount = 3;
    orig.splitMsg = 0;

    char buf[128];
    size_t len = orig.encode(buf, 0);

    Retransmission509 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.uuid, 42u);
    EXPECT_EQ(decoded.lastUUID, 41u);
    EXPECT_EQ(decoded.fromSeqNo, 10u);
    EXPECT_EQ(decoded.msgCount, 3u);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, NotApplied513Roundtrip) {
    NotApplied513 orig;
    orig.uuid = 42;
    orig.fromSeqNo = 5;
    orig.msgCount = 3;

    char buf[64];
    size_t len = orig.encode(buf, 0);

    NotApplied513 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.uuid, 42u);
    EXPECT_EQ(decoded.fromSeqNo, 5u);
    EXPECT_EQ(decoded.msgCount, 3u);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, OrderCancelReplaceRequest515Roundtrip) {
    OrderCancelReplaceRequest515 orig;
    orig.price = static_cast<int64_t>(101.5 * 1e9);
    orig.orderQty = 20;
    orig.securityID = 999;
    orig.side = 2; // Sell
    orig.seqNum = 10;
    orig.orderID = 500;
    orig.ordType = 2;

    char buf[512];
    size_t len = orig.encode(buf, 0);

    OrderCancelReplaceRequest515 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.price, orig.price);
    EXPECT_EQ(decoded.orderQty, 20u);
    EXPECT_EQ(decoded.securityID, 999);
    EXPECT_EQ(decoded.side, 2u);
    EXPECT_EQ(decoded.seqNum, 10u);
    EXPECT_EQ(decoded.orderID, 500u);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, OrderCancelRequest516Roundtrip) {
    OrderCancelRequest516 orig;
    orig.orderID = 300;
    orig.seqNum = 7;
    orig.securityID = 888;
    orig.side = 1;

    char buf[256];
    size_t len = orig.encode(buf, 0);

    OrderCancelRequest516 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.orderID, 300u);
    EXPECT_EQ(decoded.seqNum, 7u);
    EXPECT_EQ(decoded.securityID, 888);
    EXPECT_EQ(decoded.side, 1u);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, ExecutionReportCancel534Roundtrip) {
    ExecutionReportCancel534 orig;
    orig.seqNum = 3;
    orig.uuid = 42;
    orig.orderID = 100;
    orig.securityID = 555;
    orig.cumQty = 5;
    orig.orderQty = 10;
    orig.ordStatus = '4';
    orig.execType = '4';

    char buf[512];
    size_t len = orig.encode(buf, 0);

    ExecutionReportCancel534 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.seqNum, 3u);
    EXPECT_EQ(decoded.orderID, 100u);
    EXPECT_EQ(decoded.securityID, 555);
    EXPECT_EQ(decoded.cumQty, 5u);
    EXPECT_EQ(decoded.ordStatus, '4');
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, ExecutionReportModify531Roundtrip) {
    ExecutionReportModify531 orig;
    orig.seqNum = 2;
    orig.uuid = 42;
    orig.orderID = 100;
    orig.price = static_cast<int64_t>(105.0 * 1e9);
    orig.securityID = 555;
    orig.ordStatus = '0';
    orig.execType = '5';

    char buf[512];
    size_t len = orig.encode(buf, 0);

    ExecutionReportModify531 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.seqNum, 2u);
    EXPECT_EQ(decoded.orderID, 100u);
    EXPECT_EQ(decoded.price, orig.price);
    EXPECT_EQ(decoded.ordStatus, '0');
    EXPECT_EQ(decoded.execType, '5');
    EXPECT_EQ(len, orig.encodedLength());
}

// ===========================================================================
// MDP 3.0 Message Roundtrip Tests
// ===========================================================================

TEST(SBECodec, MDIncrementalRefreshBook46Roundtrip) {
    MDIncrementalRefreshBook46 orig;
    orig.transactTime = 1000000000ULL;
    orig.matchEventIndicator = 0x84; // EndOfEvent | LastQuoteMsg

    MDIncrementalRefreshBook46::Entry e1;
    e1.mdEntryPx = static_cast<int64_t>(100.25 * 1e9);
    e1.mdEntrySize = 50;
    e1.securityID = 12345;
    e1.rptSeq = 1;
    e1.numberOfOrders = 3;
    e1.mdPriceLevel = 1;
    e1.mdUpdateAction = 0; // New
    e1.mdEntryType = '0';  // Bid

    MDIncrementalRefreshBook46::Entry e2;
    e2.mdEntryPx = static_cast<int64_t>(100.50 * 1e9);
    e2.mdEntrySize = 30;
    e2.securityID = 12345;
    e2.rptSeq = 2;
    e2.numberOfOrders = 2;
    e2.mdPriceLevel = 1;
    e2.mdUpdateAction = 0;
    e2.mdEntryType = '1'; // Offer

    orig.entries.push_back(e1);
    orig.entries.push_back(e2);

    char buf[512];
    size_t len = orig.encode(buf, 0);
    EXPECT_EQ(len, orig.encodedLength());

    MDIncrementalRefreshBook46 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.transactTime, 1000000000ULL);
    EXPECT_EQ(decoded.matchEventIndicator, 0x84u);
    ASSERT_EQ(decoded.entries.size(), 2u);

    EXPECT_EQ(decoded.entries[0].mdEntryPx, e1.mdEntryPx);
    EXPECT_EQ(decoded.entries[0].mdEntrySize, 50);
    EXPECT_EQ(decoded.entries[0].securityID, 12345);
    EXPECT_EQ(decoded.entries[0].rptSeq, 1u);
    EXPECT_EQ(decoded.entries[0].numberOfOrders, 3);
    EXPECT_EQ(decoded.entries[0].mdPriceLevel, 1u);
    EXPECT_EQ(decoded.entries[0].mdUpdateAction, 0u);
    EXPECT_EQ(decoded.entries[0].mdEntryType, '0');

    EXPECT_EQ(decoded.entries[1].mdEntryPx, e2.mdEntryPx);
    EXPECT_EQ(decoded.entries[1].mdEntryType, '1');
}

TEST(SBECodec, SnapshotFullRefresh52Roundtrip) {
    SnapshotFullRefresh52 orig;
    orig.lastMsgSeqNumProcessed = 100;
    orig.totNumReports = 5;
    orig.securityID = 12345;
    orig.rptSeq = 50;
    orig.transactTime = 2000000000ULL;
    orig.lastUpdateTime = 2100000000ULL;
    orig.tradeDate = 20250315;
    orig.mdSecurityTradingStatus = 17; // Open
    orig.highLimitPrice = static_cast<int64_t>(6000.0 * 1e9);
    orig.lowLimitPrice = static_cast<int64_t>(4000.0 * 1e9);
    orig.maxPriceVariation = static_cast<int64_t>(100.0 * 1e9);

    SnapshotFullRefresh52::Entry bid;
    bid.mdEntryPx = static_cast<int64_t>(5000.0 * 1e9);
    bid.mdEntrySize = 100;
    bid.numberOfOrders = 5;
    bid.mdPriceLevel = 1;
    bid.mdEntryType = '0'; // Bid

    SnapshotFullRefresh52::Entry ask;
    ask.mdEntryPx = static_cast<int64_t>(5000.25 * 1e9);
    ask.mdEntrySize = 80;
    ask.numberOfOrders = 3;
    ask.mdPriceLevel = 1;
    ask.mdEntryType = '1'; // Offer

    orig.entries.push_back(bid);
    orig.entries.push_back(ask);

    char buf[512];
    size_t len = orig.encode(buf, 0);
    EXPECT_EQ(len, orig.encodedLength());

    SnapshotFullRefresh52 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.lastMsgSeqNumProcessed, 100u);
    EXPECT_EQ(decoded.totNumReports, 5u);
    EXPECT_EQ(decoded.securityID, 12345);
    EXPECT_EQ(decoded.rptSeq, 50u);
    EXPECT_EQ(decoded.transactTime, 2000000000ULL);
    EXPECT_EQ(decoded.lastUpdateTime, 2100000000ULL);
    EXPECT_EQ(decoded.mdSecurityTradingStatus, 17u);
    EXPECT_EQ(decoded.highLimitPrice, orig.highLimitPrice);
    EXPECT_EQ(decoded.lowLimitPrice, orig.lowLimitPrice);

    ASSERT_EQ(decoded.entries.size(), 2u);
    EXPECT_EQ(decoded.entries[0].mdEntryPx, bid.mdEntryPx);
    EXPECT_EQ(decoded.entries[0].mdEntrySize, 100);
    EXPECT_EQ(decoded.entries[0].numberOfOrders, 5);
    EXPECT_EQ(decoded.entries[0].mdPriceLevel, 1u);
    EXPECT_EQ(decoded.entries[0].mdEntryType, '0');

    EXPECT_EQ(decoded.entries[1].mdEntryPx, ask.mdEntryPx);
    EXPECT_EQ(decoded.entries[1].mdEntryType, '1');
}

TEST(SBECodec, ChannelReset4Roundtrip) {
    ChannelReset4 orig;
    orig.transactTime = 3000000000ULL;
    orig.matchEventIndicator = 0x80;
    ChannelReset4::Entry e;
    e.applID = 310;
    orig.entries.push_back(e);

    char buf[128];
    size_t len = orig.encode(buf, 0);

    ChannelReset4 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.transactTime, 3000000000ULL);
    EXPECT_EQ(decoded.matchEventIndicator, 0x80u);
    ASSERT_EQ(decoded.entries.size(), 1u);
    EXPECT_EQ(decoded.entries[0].applID, 310);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, SecurityStatus30Roundtrip) {
    SecurityStatus30 orig;
    orig.transactTime = 4000000000ULL;
    writeFixedString(orig.securityGroup, "ES", 6);
    writeFixedString(orig.asset, "ES", 6);
    orig.securityID = 12345;
    orig.tradeDate = 20250315;
    orig.securityTradingStatus = 17; // Open
    orig.haltReason = 0;

    char buf[256];
    size_t len = orig.encode(buf, 0);

    SecurityStatus30 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.transactTime, 4000000000ULL);
    EXPECT_EQ(decoded.securityID, 12345);
    EXPECT_EQ(decoded.securityTradingStatus, 17u);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, MDIncrementalRefreshTradeSummary48Roundtrip) {
    MDIncrementalRefreshTradeSummary48 orig;
    orig.transactTime = 5000000000ULL;
    orig.matchEventIndicator = 0x01;

    MDIncrementalRefreshTradeSummary48::MDEntry me;
    me.mdEntryPx = static_cast<int64_t>(5000.0 * 1e9);
    me.mdEntrySize = 10;
    me.securityID = 12345;
    me.rptSeq = 1;
    me.numberOfOrders = 2;
    me.aggressorSide = 1;
    me.mdUpdateAction = 0;
    orig.mdEntries.push_back(me);

    MDIncrementalRefreshTradeSummary48::OrderIDEntry oe;
    oe.orderID = 42;
    oe.lastQty = 10;
    orig.orderIDEntries.push_back(oe);

    char buf[512];
    size_t len = orig.encode(buf, 0);

    MDIncrementalRefreshTradeSummary48 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.transactTime, 5000000000ULL);
    ASSERT_EQ(decoded.mdEntries.size(), 1u);
    EXPECT_EQ(decoded.mdEntries[0].mdEntryPx, me.mdEntryPx);
    EXPECT_EQ(decoded.mdEntries[0].mdEntrySize, 10);
    EXPECT_EQ(decoded.mdEntries[0].aggressorSide, 1u);

    ASSERT_EQ(decoded.orderIDEntries.size(), 1u);
    EXPECT_EQ(decoded.orderIDEntries[0].orderID, 42u);
    EXPECT_EQ(decoded.orderIDEntries[0].lastQty, 10);
    EXPECT_EQ(len, orig.encodedLength());
}

TEST(SBECodec, AdminHeartbeat12Roundtrip) {
    AdminHeartbeat12 orig;

    char buf[32];
    size_t len = orig.encode(buf, 0);

    // Should just be a MessageHeader with blockLength=0
    EXPECT_EQ(len, MessageHeader::SIZE);
    EXPECT_EQ(MessageHeader::decodeTemplateId(buf), AdminHeartbeat12::TEMPLATE_ID);
    EXPECT_EQ(MessageHeader::decodeBlockLength(buf), 0u);
}

// ===========================================================================
// Price Encoding Tests
// ===========================================================================

TEST(SBECodec, PriceFixedPointEncoding) {
    using cme::sim::Price;

    // 100.0 -> mantissa = 100,000,000,000
    Price p = Price::fromDouble(100.0);
    EXPECT_EQ(p.mantissa, 100000000000LL);
    EXPECT_NEAR(p.toDouble(), 100.0, 1e-6);

    // 0.25 (ES tick)
    Price tick = Price::fromDouble(0.25);
    EXPECT_NEAR(tick.toDouble(), 0.25, 1e-6);

    // 5432.75
    Price px = Price::fromDouble(5432.75);
    EXPECT_NEAR(px.toDouble(), 5432.75, 1e-6);

    // Null price
    Price null_px = Price::null();
    EXPECT_TRUE(null_px.isNull());

    // Arithmetic
    Price a = Price::fromDouble(100.0);
    Price b = Price::fromDouble(0.25);
    Price c = a + b;
    EXPECT_NEAR(c.toDouble(), 100.25, 1e-6);

    Price d = a - b;
    EXPECT_NEAR(d.toDouble(), 99.75, 1e-6);
}

TEST(SBECodec, PriceComparison) {
    using cme::sim::Price;

    Price p100 = Price::fromDouble(100.0);
    Price p101 = Price::fromDouble(101.0);
    Price p100b = Price::fromDouble(100.0);

    EXPECT_TRUE(p100 < p101);
    EXPECT_TRUE(p101 > p100);
    EXPECT_TRUE(p100 == p100b);
    EXPECT_TRUE(p100 != p101);
    EXPECT_TRUE(p100 <= p101);
    EXPECT_TRUE(p100 <= p100b);
    EXPECT_TRUE(p101 >= p100);
}

// ===========================================================================
// GroupSize Tests
// ===========================================================================

TEST(SBECodec, GroupSizeRoundtrip) {
    char buf[GroupSize::SIZE];
    GroupSize::encode(buf, 27, 5);

    EXPECT_EQ(GroupSize::decodeBlockLength(buf), 27u);
    EXPECT_EQ(GroupSize::decodeNumInGroup(buf), 5u);
}

TEST(SBECodec, EmptyGroup) {
    MDIncrementalRefreshBook46 orig;
    orig.transactTime = 1ULL;
    orig.matchEventIndicator = 0;
    // No entries

    char buf[128];
    size_t len = orig.encode(buf, 0);

    MDIncrementalRefreshBook46 decoded;
    decoded.decode(buf, 0);

    EXPECT_EQ(decoded.entries.size(), 0u);
    EXPECT_EQ(len, orig.encodedLength());
}
