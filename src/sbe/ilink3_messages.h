#pragma once
#include <cstdint>
#include <cstring>
#include "message_header.h"

namespace cme::sim::sbe {

// Helper to write a fixed-size string field (zero-padded)
inline void writeFixedString(char* dest, const char* src, size_t len) {
    std::memset(dest, 0, len);
    if (src) {
        size_t slen = std::strlen(src);
        if (slen > len) slen = len;
        std::memcpy(dest, src, slen);
    }
}

// Helper to read a fixed-size string field into a null-terminated buffer
inline void readFixedString(char* dest, const char* src, size_t len) {
    std::memcpy(dest, src, len);
    dest[len] = '\0';
}

// ============================================================================
// Negotiate (templateId=500)
// ============================================================================
struct Negotiate500 {
    static constexpr uint16_t TEMPLATE_ID = 500;
    static constexpr uint16_t BLOCK_LENGTH = 76; // 8+32+20+3+5+8 = 76 (without CancelOnDisconnect fields which follow)

    // Actual layout: total fixed block
    // Offset  Field
    //   0     HMACSignature[32]
    //  32     AccessKeyID[20]
    //  52     UUID(8)
    //  60     RequestTimestamp(8) - SendingTime in spec
    //  68     Session[3]
    //  71     Firm[5]
    //  76     CancelOnDisconnectIndicator(1)
    //  77     CancelOnRejectConID(8)
    // Total BLOCK_LENGTH = 85
    static constexpr uint16_t ACTUAL_BLOCK_LENGTH = 85;

    char hmacSignature[32]{};
    char accessKeyID[20]{};
    uint64_t uuid = 0;
    uint64_t sendingTime = 0;
    char session[3]{};
    char firm[5]{};
    uint8_t cancelOnDisconnectIndicator = 0;
    uint64_t cancelOnRejectConID = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, ACTUAL_BLOCK_LENGTH, TEMPLATE_ID);
        char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(body + 0, hmacSignature, 32);
        std::memcpy(body + 32, accessKeyID, 20);
        std::memcpy(body + 52, &uuid, 8);
        std::memcpy(body + 60, &sendingTime, 8);
        std::memcpy(body + 68, session, 3);
        std::memcpy(body + 71, firm, 5);
        std::memcpy(body + 76, &cancelOnDisconnectIndicator, 1);
        std::memcpy(body + 77, &cancelOnRejectConID, 8);
        return MessageHeader::SIZE + ACTUAL_BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(hmacSignature, body + 0, 32);
        std::memcpy(accessKeyID, body + 32, 20);
        std::memcpy(&uuid, body + 52, 8);
        std::memcpy(&sendingTime, body + 60, 8);
        std::memcpy(session, body + 68, 3);
        std::memcpy(firm, body + 71, 5);
        std::memcpy(&cancelOnDisconnectIndicator, body + 76, 1);
        std::memcpy(&cancelOnRejectConID, body + 77, 8);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + ACTUAL_BLOCK_LENGTH; }
};

// ============================================================================
// NegotiationResponse (templateId=501)
// ============================================================================
struct NegotiationResponse501 {
    static constexpr uint16_t TEMPLATE_ID = 501;
    // Offset  Field
    //   0     UUID(8)
    //   8     RequestTimestamp(8)
    //  16     SecretKeySecureIDExpiration(2)
    //  18     FaultToleranceIndicator(1)
    //  19     SplitMsg(1)
    //  20     PreviousSeqNo(4)
    //  24     PreviousUUID(8)
    static constexpr uint16_t BLOCK_LENGTH = 32;

    uint64_t uuid = 0;
    uint64_t requestTimestamp = 0;
    uint16_t secretKeySecureIDExpiration = 0;
    uint8_t faultToleranceIndicator = 0;
    uint8_t splitMsg = 0;
    uint32_t previousSeqNo = 0;
    uint64_t previousUUID = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(body + 0, &uuid, 8);
        std::memcpy(body + 8, &requestTimestamp, 8);
        std::memcpy(body + 16, &secretKeySecureIDExpiration, 2);
        std::memcpy(body + 18, &faultToleranceIndicator, 1);
        std::memcpy(body + 19, &splitMsg, 1);
        std::memcpy(body + 20, &previousSeqNo, 4);
        std::memcpy(body + 24, &previousUUID, 8);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&uuid, body + 0, 8);
        std::memcpy(&requestTimestamp, body + 8, 8);
        std::memcpy(&secretKeySecureIDExpiration, body + 16, 2);
        std::memcpy(&faultToleranceIndicator, body + 18, 1);
        std::memcpy(&splitMsg, body + 19, 1);
        std::memcpy(&previousSeqNo, body + 20, 4);
        std::memcpy(&previousUUID, body + 24, 8);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// Establish (templateId=503)
// ============================================================================
struct Establish503 {
    static constexpr uint16_t TEMPLATE_ID = 503;
    // Offset  Field
    //   0     HMACSignature[32]
    //  32     AccessKeyID[20]
    //  52     UUID(8)
    //  60     SendingTime(8)
    //  68     Session[3]
    //  71     Firm[5]
    //  76     KeepAliveInterval(2)
    //  78     NextSeqNo(4)
    //  82     CancelOnDisconnectIndicator(1)
    static constexpr uint16_t BLOCK_LENGTH = 83;

    char hmacSignature[32]{};
    char accessKeyID[20]{};
    uint64_t uuid = 0;
    uint64_t sendingTime = 0;
    char session[3]{};
    char firm[5]{};
    uint16_t keepAliveInterval = 0;
    uint32_t nextSeqNo = 0;
    uint8_t cancelOnDisconnectIndicator = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(body + 0, hmacSignature, 32);
        std::memcpy(body + 32, accessKeyID, 20);
        std::memcpy(body + 52, &uuid, 8);
        std::memcpy(body + 60, &sendingTime, 8);
        std::memcpy(body + 68, session, 3);
        std::memcpy(body + 71, firm, 5);
        std::memcpy(body + 76, &keepAliveInterval, 2);
        std::memcpy(body + 78, &nextSeqNo, 4);
        std::memcpy(body + 82, &cancelOnDisconnectIndicator, 1);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(hmacSignature, body + 0, 32);
        std::memcpy(accessKeyID, body + 32, 20);
        std::memcpy(&uuid, body + 52, 8);
        std::memcpy(&sendingTime, body + 60, 8);
        std::memcpy(session, body + 68, 3);
        std::memcpy(firm, body + 71, 5);
        std::memcpy(&keepAliveInterval, body + 76, 2);
        std::memcpy(&nextSeqNo, body + 78, 4);
        std::memcpy(&cancelOnDisconnectIndicator, body + 82, 1);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// EstablishmentAck (templateId=504)
// ============================================================================
struct EstablishmentAck504 {
    static constexpr uint16_t TEMPLATE_ID = 504;
    // Offset  Field
    //   0     UUID(8)
    //   8     RequestTimestamp(8)
    //  16     KeepAliveInterval(2)
    //  18     NextSeqNo(4)
    //  22     PreviousSeqNo(4)
    //  26     PreviousUUID(8)
    static constexpr uint16_t BLOCK_LENGTH = 34;

    uint64_t uuid = 0;
    uint64_t requestTimestamp = 0;
    uint16_t keepAliveInterval = 0;
    uint32_t nextSeqNo = 0;
    uint32_t previousSeqNo = 0;
    uint64_t previousUUID = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(body + 0, &uuid, 8);
        std::memcpy(body + 8, &requestTimestamp, 8);
        std::memcpy(body + 16, &keepAliveInterval, 2);
        std::memcpy(body + 18, &nextSeqNo, 4);
        std::memcpy(body + 22, &previousSeqNo, 4);
        std::memcpy(body + 26, &previousUUID, 8);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&uuid, body + 0, 8);
        std::memcpy(&requestTimestamp, body + 8, 8);
        std::memcpy(&keepAliveInterval, body + 16, 2);
        std::memcpy(&nextSeqNo, body + 18, 4);
        std::memcpy(&previousSeqNo, body + 22, 4);
        std::memcpy(&previousUUID, body + 26, 8);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// Sequence (templateId=506)
// ============================================================================
struct Sequence506 {
    static constexpr uint16_t TEMPLATE_ID = 506;
    //   0     UUID(8)
    //   8     NextSeqNo(4)
    //  12     FaultToleranceIndicator(1)
    //  13     KeepAliveIntervalLapsed(1)
    static constexpr uint16_t BLOCK_LENGTH = 14;

    uint64_t uuid = 0;
    uint32_t nextSeqNo = 0;
    uint8_t faultToleranceIndicator = 0;
    uint8_t keepAliveIntervalLapsed = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(body + 0, &uuid, 8);
        std::memcpy(body + 8, &nextSeqNo, 4);
        std::memcpy(body + 12, &faultToleranceIndicator, 1);
        std::memcpy(body + 13, &keepAliveIntervalLapsed, 1);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&uuid, body + 0, 8);
        std::memcpy(&nextSeqNo, body + 8, 4);
        std::memcpy(&faultToleranceIndicator, body + 12, 1);
        std::memcpy(&keepAliveIntervalLapsed, body + 13, 1);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// Terminate (templateId=507)
// ============================================================================
struct Terminate507 {
    static constexpr uint16_t TEMPLATE_ID = 507;
    //   0     UUID(8)
    //   8     RequestTimestamp(8)
    //  16     ErrorCodes(2)
    //  18     SplitMsg(1)
    static constexpr uint16_t BLOCK_LENGTH = 19;

    uint64_t uuid = 0;
    uint64_t requestTimestamp = 0;
    uint16_t errorCodes = 0;
    uint8_t splitMsg = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(body + 0, &uuid, 8);
        std::memcpy(body + 8, &requestTimestamp, 8);
        std::memcpy(body + 16, &errorCodes, 2);
        std::memcpy(body + 18, &splitMsg, 1);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&uuid, body + 0, 8);
        std::memcpy(&requestTimestamp, body + 8, 8);
        std::memcpy(&errorCodes, body + 16, 2);
        std::memcpy(&splitMsg, body + 18, 1);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// RetransmitRequest (templateId=508)
// ============================================================================
struct RetransmitRequest508 {
    static constexpr uint16_t TEMPLATE_ID = 508;
    //   0     UUID(8)
    //   8     LastUUID(8)
    //  16     RequestTimestamp(8)
    //  24     FromSeqNo(4)
    //  28     MsgCount(2)
    static constexpr uint16_t BLOCK_LENGTH = 30;

    uint64_t uuid = 0;
    uint64_t lastUUID = 0;
    uint64_t requestTimestamp = 0;
    uint32_t fromSeqNo = 0;
    uint16_t msgCount = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(body + 0, &uuid, 8);
        std::memcpy(body + 8, &lastUUID, 8);
        std::memcpy(body + 16, &requestTimestamp, 8);
        std::memcpy(body + 24, &fromSeqNo, 4);
        std::memcpy(body + 28, &msgCount, 2);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&uuid, body + 0, 8);
        std::memcpy(&lastUUID, body + 8, 8);
        std::memcpy(&requestTimestamp, body + 16, 8);
        std::memcpy(&fromSeqNo, body + 24, 4);
        std::memcpy(&msgCount, body + 28, 2);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// Retransmission (templateId=509)
// ============================================================================
struct Retransmission509 {
    static constexpr uint16_t TEMPLATE_ID = 509;
    //   0     UUID(8)
    //   8     LastUUID(8)
    //  16     RequestTimestamp(8)
    //  24     FromSeqNo(4)
    //  28     MsgCount(2)
    //  30     SplitMsg(1)
    static constexpr uint16_t BLOCK_LENGTH = 31;

    uint64_t uuid = 0;
    uint64_t lastUUID = 0;
    uint64_t requestTimestamp = 0;
    uint32_t fromSeqNo = 0;
    uint16_t msgCount = 0;
    uint8_t splitMsg = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(body + 0, &uuid, 8);
        std::memcpy(body + 8, &lastUUID, 8);
        std::memcpy(body + 16, &requestTimestamp, 8);
        std::memcpy(body + 24, &fromSeqNo, 4);
        std::memcpy(body + 28, &msgCount, 2);
        std::memcpy(body + 30, &splitMsg, 1);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&uuid, body + 0, 8);
        std::memcpy(&lastUUID, body + 8, 8);
        std::memcpy(&requestTimestamp, body + 16, 8);
        std::memcpy(&fromSeqNo, body + 24, 4);
        std::memcpy(&msgCount, body + 28, 2);
        std::memcpy(&splitMsg, body + 30, 1);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// NotApplied (templateId=513)
// ============================================================================
struct NotApplied513 {
    static constexpr uint16_t TEMPLATE_ID = 513;
    //   0     UUID(8)
    //   8     FromSeqNo(4)
    //  12     MsgCount(4)
    static constexpr uint16_t BLOCK_LENGTH = 16;

    uint64_t uuid = 0;
    uint32_t fromSeqNo = 0;
    uint32_t msgCount = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(body + 0, &uuid, 8);
        std::memcpy(body + 8, &fromSeqNo, 4);
        std::memcpy(body + 12, &msgCount, 4);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* body = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&uuid, body + 0, 8);
        std::memcpy(&fromSeqNo, body + 8, 4);
        std::memcpy(&msgCount, body + 12, 4);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// NewOrderSingle (templateId=514)
// ============================================================================
struct NewOrderSingle514 {
    static constexpr uint16_t TEMPLATE_ID = 514;
    // Field layout (packed):
    //   0  Price(8)
    //   8  OrderQty(4)
    //  12  SecurityID(4)
    //  16  Side(1)
    //  17  SeqNum(4)
    //  21  SenderID[20]
    //  41  ClOrdID[20]
    //  61  PartyDetailsListReqID(8)
    //  69  OrderRequestID(8)
    //  77  SendingTimeEpoch(8)
    //  85  StopPx(8)
    //  93  Location[5]
    //  98  MinQty(4)
    // 102  DisplayQty(4)
    // 106  ExpireDate(2)
    // 108  OrdType(1)
    // 109  TimeInForce(1)
    // 110  ManualOrderIndicator(1)
    // 111  ExecInst(1)
    // 112  ExecutionMode(1)
    // 113  LiquidityFlag(1)
    // 114  ManagedOrder(1)
    // 115  ShortSaleType(1)
    static constexpr uint16_t BLOCK_LENGTH = 116;

    int64_t price = 0;
    uint32_t orderQty = 0;
    int32_t securityID = 0;
    uint8_t side = 0;
    uint32_t seqNum = 0;
    char senderID[20]{};
    char clOrdID[20]{};
    uint64_t partyDetailsListReqID = 0;
    uint64_t orderRequestID = 0;
    uint64_t sendingTimeEpoch = 0;
    int64_t stopPx = 0;
    char location[5]{};
    uint32_t minQty = 0;
    uint32_t displayQty = 0;
    uint16_t expireDate = 0;
    uint8_t ordType = 0;
    uint8_t timeInForce = 0;
    uint8_t manualOrderIndicator = 0;
    char execInst = '\0';
    char executionMode = '\0';
    uint8_t liquidityFlag = 0;
    uint8_t managedOrder = 0;
    uint8_t shortSaleType = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,   &price, 8);
        std::memcpy(b + 8,   &orderQty, 4);
        std::memcpy(b + 12,  &securityID, 4);
        std::memcpy(b + 16,  &side, 1);
        std::memcpy(b + 17,  &seqNum, 4);
        std::memcpy(b + 21,  senderID, 20);
        std::memcpy(b + 41,  clOrdID, 20);
        std::memcpy(b + 61,  &partyDetailsListReqID, 8);
        std::memcpy(b + 69,  &orderRequestID, 8);
        std::memcpy(b + 77,  &sendingTimeEpoch, 8);
        std::memcpy(b + 85,  &stopPx, 8);
        std::memcpy(b + 93,  location, 5);
        std::memcpy(b + 98,  &minQty, 4);
        std::memcpy(b + 102, &displayQty, 4);
        std::memcpy(b + 106, &expireDate, 2);
        std::memcpy(b + 108, &ordType, 1);
        std::memcpy(b + 109, &timeInForce, 1);
        std::memcpy(b + 110, &manualOrderIndicator, 1);
        std::memcpy(b + 111, &execInst, 1);
        std::memcpy(b + 112, &executionMode, 1);
        std::memcpy(b + 113, &liquidityFlag, 1);
        std::memcpy(b + 114, &managedOrder, 1);
        std::memcpy(b + 115, &shortSaleType, 1);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&price, b + 0, 8);
        std::memcpy(&orderQty, b + 8, 4);
        std::memcpy(&securityID, b + 12, 4);
        std::memcpy(&side, b + 16, 1);
        std::memcpy(&seqNum, b + 17, 4);
        std::memcpy(senderID, b + 21, 20);
        std::memcpy(clOrdID, b + 41, 20);
        std::memcpy(&partyDetailsListReqID, b + 61, 8);
        std::memcpy(&orderRequestID, b + 69, 8);
        std::memcpy(&sendingTimeEpoch, b + 77, 8);
        std::memcpy(&stopPx, b + 85, 8);
        std::memcpy(location, b + 93, 5);
        std::memcpy(&minQty, b + 98, 4);
        std::memcpy(&displayQty, b + 102, 4);
        std::memcpy(&expireDate, b + 106, 2);
        std::memcpy(&ordType, b + 108, 1);
        std::memcpy(&timeInForce, b + 109, 1);
        std::memcpy(&manualOrderIndicator, b + 110, 1);
        std::memcpy(&execInst, b + 111, 1);
        std::memcpy(&executionMode, b + 112, 1);
        std::memcpy(&liquidityFlag, b + 113, 1);
        std::memcpy(&managedOrder, b + 114, 1);
        std::memcpy(&shortSaleType, b + 115, 1);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// OrderCancelReplaceRequest (templateId=515)
// ============================================================================
struct OrderCancelReplaceRequest515 {
    static constexpr uint16_t TEMPLATE_ID = 515;
    //   0  Price(8)
    //   8  OrderQty(4)
    //  12  SecurityID(4)
    //  16  Side(1)
    //  17  SeqNum(4)
    //  21  SenderID[20]
    //  41  ClOrdID[20]
    //  61  PartyDetailsListReqID(8)
    //  69  OrderID(8)
    //  77  StopPx(8)
    //  85  OrderRequestID(8)
    //  93  SendingTimeEpoch(8)
    // 101  Location[5]
    // 106  MinQty(4)
    // 110  DisplayQty(4)
    // 114  ExpireDate(2)
    // 116  OrdType(1)
    // 117  TimeInForce(1)
    // 118  ManualOrderIndicator(1)
    // 119  OFMOverride(1)
    // 120  ExecInst(1)
    // 121  ExecutionMode(1)
    // 122  LiquidityFlag(1)
    // 123  ManagedOrder(1)
    // 124  ShortSaleType(1)
    static constexpr uint16_t BLOCK_LENGTH = 125;

    int64_t price = 0;
    uint32_t orderQty = 0;
    int32_t securityID = 0;
    uint8_t side = 0;
    uint32_t seqNum = 0;
    char senderID[20]{};
    char clOrdID[20]{};
    uint64_t partyDetailsListReqID = 0;
    uint64_t orderID = 0;
    int64_t stopPx = 0;
    uint64_t orderRequestID = 0;
    uint64_t sendingTimeEpoch = 0;
    char location[5]{};
    uint32_t minQty = 0;
    uint32_t displayQty = 0;
    uint16_t expireDate = 0;
    uint8_t ordType = 0;
    uint8_t timeInForce = 0;
    uint8_t manualOrderIndicator = 0;
    uint8_t ofmOverride = 0;
    char execInst = '\0';
    char executionMode = '\0';
    uint8_t liquidityFlag = 0;
    uint8_t managedOrder = 0;
    uint8_t shortSaleType = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,   &price, 8);
        std::memcpy(b + 8,   &orderQty, 4);
        std::memcpy(b + 12,  &securityID, 4);
        std::memcpy(b + 16,  &side, 1);
        std::memcpy(b + 17,  &seqNum, 4);
        std::memcpy(b + 21,  senderID, 20);
        std::memcpy(b + 41,  clOrdID, 20);
        std::memcpy(b + 61,  &partyDetailsListReqID, 8);
        std::memcpy(b + 69,  &orderID, 8);
        std::memcpy(b + 77,  &stopPx, 8);
        std::memcpy(b + 85,  &orderRequestID, 8);
        std::memcpy(b + 93,  &sendingTimeEpoch, 8);
        std::memcpy(b + 101, location, 5);
        std::memcpy(b + 106, &minQty, 4);
        std::memcpy(b + 110, &displayQty, 4);
        std::memcpy(b + 114, &expireDate, 2);
        std::memcpy(b + 116, &ordType, 1);
        std::memcpy(b + 117, &timeInForce, 1);
        std::memcpy(b + 118, &manualOrderIndicator, 1);
        std::memcpy(b + 119, &ofmOverride, 1);
        std::memcpy(b + 120, &execInst, 1);
        std::memcpy(b + 121, &executionMode, 1);
        std::memcpy(b + 122, &liquidityFlag, 1);
        std::memcpy(b + 123, &managedOrder, 1);
        std::memcpy(b + 124, &shortSaleType, 1);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&price, b + 0, 8);
        std::memcpy(&orderQty, b + 8, 4);
        std::memcpy(&securityID, b + 12, 4);
        std::memcpy(&side, b + 16, 1);
        std::memcpy(&seqNum, b + 17, 4);
        std::memcpy(senderID, b + 21, 20);
        std::memcpy(clOrdID, b + 41, 20);
        std::memcpy(&partyDetailsListReqID, b + 61, 8);
        std::memcpy(&orderID, b + 69, 8);
        std::memcpy(&stopPx, b + 77, 8);
        std::memcpy(&orderRequestID, b + 85, 8);
        std::memcpy(&sendingTimeEpoch, b + 93, 8);
        std::memcpy(location, b + 101, 5);
        std::memcpy(&minQty, b + 106, 4);
        std::memcpy(&displayQty, b + 110, 4);
        std::memcpy(&expireDate, b + 114, 2);
        std::memcpy(&ordType, b + 116, 1);
        std::memcpy(&timeInForce, b + 117, 1);
        std::memcpy(&manualOrderIndicator, b + 118, 1);
        std::memcpy(&ofmOverride, b + 119, 1);
        std::memcpy(&execInst, b + 120, 1);
        std::memcpy(&executionMode, b + 121, 1);
        std::memcpy(&liquidityFlag, b + 122, 1);
        std::memcpy(&managedOrder, b + 123, 1);
        std::memcpy(&shortSaleType, b + 124, 1);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// OrderCancelRequest (templateId=516)
// ============================================================================
struct OrderCancelRequest516 {
    static constexpr uint16_t TEMPLATE_ID = 516;
    //   0  OrderID(8)
    //   8  PartyDetailsListReqID(8)
    //  16  ManualOrderIndicator(1)
    //  17  SeqNum(4)
    //  21  SenderID[20]
    //  41  ClOrdID[20]
    //  61  SecurityID(4)
    //  65  Side(1)
    //  66  OrderRequestID(8)
    //  74  SendingTimeEpoch(8)
    //  82  Location[5]
    //  87  LiquidityFlag(1)
    static constexpr uint16_t BLOCK_LENGTH = 88;

    uint64_t orderID = 0;
    uint64_t partyDetailsListReqID = 0;
    uint8_t manualOrderIndicator = 0;
    uint32_t seqNum = 0;
    char senderID[20]{};
    char clOrdID[20]{};
    int32_t securityID = 0;
    uint8_t side = 0;
    uint64_t orderRequestID = 0;
    uint64_t sendingTimeEpoch = 0;
    char location[5]{};
    uint8_t liquidityFlag = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,  &orderID, 8);
        std::memcpy(b + 8,  &partyDetailsListReqID, 8);
        std::memcpy(b + 16, &manualOrderIndicator, 1);
        std::memcpy(b + 17, &seqNum, 4);
        std::memcpy(b + 21, senderID, 20);
        std::memcpy(b + 41, clOrdID, 20);
        std::memcpy(b + 61, &securityID, 4);
        std::memcpy(b + 65, &side, 1);
        std::memcpy(b + 66, &orderRequestID, 8);
        std::memcpy(b + 74, &sendingTimeEpoch, 8);
        std::memcpy(b + 82, location, 5);
        std::memcpy(b + 87, &liquidityFlag, 1);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&orderID, b + 0, 8);
        std::memcpy(&partyDetailsListReqID, b + 8, 8);
        std::memcpy(&manualOrderIndicator, b + 16, 1);
        std::memcpy(&seqNum, b + 17, 4);
        std::memcpy(senderID, b + 21, 20);
        std::memcpy(clOrdID, b + 41, 20);
        std::memcpy(&securityID, b + 61, 4);
        std::memcpy(&side, b + 65, 1);
        std::memcpy(&orderRequestID, b + 66, 8);
        std::memcpy(&sendingTimeEpoch, b + 74, 8);
        std::memcpy(location, b + 82, 5);
        std::memcpy(&liquidityFlag, b + 87, 1);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// ExecutionReportNew (templateId=522)
// ============================================================================
struct ExecutionReportNew522 {
    static constexpr uint16_t TEMPLATE_ID = 522;
    //   0  SeqNum(4)
    //   4  UUID(8)
    //  12  ExecID[40]
    //  52  SenderID[20]
    //  72  ClOrdID[20]
    //  92  PartyDetailsListReqID(8)
    // 100  OrderID(8)
    // 108  Price(8)
    // 116  StopPx(8)
    // 124  TransactTime(8)
    // 132  SendingTimeEpoch(8)
    // 140  OrderRequestID(8)
    // 148  Location[5]
    // 153  SecurityID(4)
    // 157  OrderQty(4)
    // 161  MinQty(4)
    // 165  DisplayQty(4)
    // 169  OrdType(1)
    // 170  Side(1)
    // 171  TimeInForce(1)
    // 172  ManualOrderIndicator(1)
    // 173  ExecInst(1)
    // 174  ExecutionMode(1)
    // 175  LiquidityFlag(1)
    // 176  ManagedOrder(1)
    // 177  ShortSaleType(1)
    // 178  ExpireDate(2)
    static constexpr uint16_t BLOCK_LENGTH = 180;

    uint32_t seqNum = 0;
    uint64_t uuid = 0;
    char execID[40]{};
    char senderID[20]{};
    char clOrdID[20]{};
    uint64_t partyDetailsListReqID = 0;
    uint64_t orderID = 0;
    int64_t price = 0;
    int64_t stopPx = 0;
    uint64_t transactTime = 0;
    uint64_t sendingTimeEpoch = 0;
    uint64_t orderRequestID = 0;
    char location[5]{};
    int32_t securityID = 0;
    uint32_t orderQty = 0;
    uint32_t minQty = 0;
    uint32_t displayQty = 0;
    uint8_t ordType = 0;
    uint8_t side = 0;
    uint8_t timeInForce = 0;
    uint8_t manualOrderIndicator = 0;
    char execInst = '\0';
    char executionMode = '\0';
    uint8_t liquidityFlag = 0;
    uint8_t managedOrder = 0;
    uint8_t shortSaleType = 0;
    uint16_t expireDate = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,   &seqNum, 4);
        std::memcpy(b + 4,   &uuid, 8);
        std::memcpy(b + 12,  execID, 40);
        std::memcpy(b + 52,  senderID, 20);
        std::memcpy(b + 72,  clOrdID, 20);
        std::memcpy(b + 92,  &partyDetailsListReqID, 8);
        std::memcpy(b + 100, &orderID, 8);
        std::memcpy(b + 108, &price, 8);
        std::memcpy(b + 116, &stopPx, 8);
        std::memcpy(b + 124, &transactTime, 8);
        std::memcpy(b + 132, &sendingTimeEpoch, 8);
        std::memcpy(b + 140, &orderRequestID, 8);
        std::memcpy(b + 148, location, 5);
        std::memcpy(b + 153, &securityID, 4);
        std::memcpy(b + 157, &orderQty, 4);
        std::memcpy(b + 161, &minQty, 4);
        std::memcpy(b + 165, &displayQty, 4);
        std::memcpy(b + 169, &ordType, 1);
        std::memcpy(b + 170, &side, 1);
        std::memcpy(b + 171, &timeInForce, 1);
        std::memcpy(b + 172, &manualOrderIndicator, 1);
        std::memcpy(b + 173, &execInst, 1);
        std::memcpy(b + 174, &executionMode, 1);
        std::memcpy(b + 175, &liquidityFlag, 1);
        std::memcpy(b + 176, &managedOrder, 1);
        std::memcpy(b + 177, &shortSaleType, 1);
        std::memcpy(b + 178, &expireDate, 2);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&seqNum, b + 0, 4);
        std::memcpy(&uuid, b + 4, 8);
        std::memcpy(execID, b + 12, 40);
        std::memcpy(senderID, b + 52, 20);
        std::memcpy(clOrdID, b + 72, 20);
        std::memcpy(&partyDetailsListReqID, b + 92, 8);
        std::memcpy(&orderID, b + 100, 8);
        std::memcpy(&price, b + 108, 8);
        std::memcpy(&stopPx, b + 116, 8);
        std::memcpy(&transactTime, b + 124, 8);
        std::memcpy(&sendingTimeEpoch, b + 132, 8);
        std::memcpy(&orderRequestID, b + 140, 8);
        std::memcpy(location, b + 148, 5);
        std::memcpy(&securityID, b + 153, 4);
        std::memcpy(&orderQty, b + 157, 4);
        std::memcpy(&minQty, b + 161, 4);
        std::memcpy(&displayQty, b + 165, 4);
        std::memcpy(&ordType, b + 169, 1);
        std::memcpy(&side, b + 170, 1);
        std::memcpy(&timeInForce, b + 171, 1);
        std::memcpy(&manualOrderIndicator, b + 172, 1);
        std::memcpy(&execInst, b + 173, 1);
        std::memcpy(&executionMode, b + 174, 1);
        std::memcpy(&liquidityFlag, b + 175, 1);
        std::memcpy(&managedOrder, b + 176, 1);
        std::memcpy(&shortSaleType, b + 177, 1);
        std::memcpy(&expireDate, b + 178, 2);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// ExecutionReportReject (templateId=523)
// ============================================================================
struct ExecutionReportReject523 {
    static constexpr uint16_t TEMPLATE_ID = 523;
    //   0  SeqNum(4)
    //   4  UUID(8)
    //  12  ExecID[40]
    //  52  SenderID[20]
    //  72  ClOrdID[20]
    //  92  PartyDetailsListReqID(8)
    // 100  OrderID(8)
    // 108  Price(8)
    // 116  StopPx(8)
    // 124  TransactTime(8)
    // 132  SendingTimeEpoch(8)
    // 140  OrderRequestID(8)
    // 148  Location[5]
    // 153  SecurityID(4)
    // 157  OrderQty(4)
    // 161  MinQty(4)
    // 165  DisplayQty(4)
    // 169  OrdRejReason(2)
    // 171  OrdType(1)
    // 172  Side(1)
    // 173  TimeInForce(1)
    // 174  ManualOrderIndicator(1)
    // 175  ExecInst(1)
    // 176  ExecutionMode(1)
    // 177  LiquidityFlag(1)
    // 178  ManagedOrder(1)
    // 179  ShortSaleType(1)
    // 180  OrdStatus(1) = '8' (Rejected)
    // 181  ExecType(1) = '8'
    // 182  ExpireDate(2)
    // 184  Text[256] -- variable, but we include a fixed portion
    static constexpr uint16_t BLOCK_LENGTH = 184;
    static constexpr size_t TEXT_MAX_LEN = 256;

    uint32_t seqNum = 0;
    uint64_t uuid = 0;
    char execID[40]{};
    char senderID[20]{};
    char clOrdID[20]{};
    uint64_t partyDetailsListReqID = 0;
    uint64_t orderID = 0;
    int64_t price = 0;
    int64_t stopPx = 0;
    uint64_t transactTime = 0;
    uint64_t sendingTimeEpoch = 0;
    uint64_t orderRequestID = 0;
    char location[5]{};
    int32_t securityID = 0;
    uint32_t orderQty = 0;
    uint32_t minQty = 0;
    uint32_t displayQty = 0;
    uint16_t ordRejReason = 0;
    uint8_t ordType = 0;
    uint8_t side = 0;
    uint8_t timeInForce = 0;
    uint8_t manualOrderIndicator = 0;
    char execInst = '\0';
    char executionMode = '\0';
    uint8_t liquidityFlag = 0;
    uint8_t managedOrder = 0;
    uint8_t shortSaleType = 0;
    char ordStatus = '8';
    char execType = '8';
    uint16_t expireDate = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,   &seqNum, 4);
        std::memcpy(b + 4,   &uuid, 8);
        std::memcpy(b + 12,  execID, 40);
        std::memcpy(b + 52,  senderID, 20);
        std::memcpy(b + 72,  clOrdID, 20);
        std::memcpy(b + 92,  &partyDetailsListReqID, 8);
        std::memcpy(b + 100, &orderID, 8);
        std::memcpy(b + 108, &price, 8);
        std::memcpy(b + 116, &stopPx, 8);
        std::memcpy(b + 124, &transactTime, 8);
        std::memcpy(b + 132, &sendingTimeEpoch, 8);
        std::memcpy(b + 140, &orderRequestID, 8);
        std::memcpy(b + 148, location, 5);
        std::memcpy(b + 153, &securityID, 4);
        std::memcpy(b + 157, &orderQty, 4);
        std::memcpy(b + 161, &minQty, 4);
        std::memcpy(b + 165, &displayQty, 4);
        std::memcpy(b + 169, &ordRejReason, 2);
        std::memcpy(b + 171, &ordType, 1);
        std::memcpy(b + 172, &side, 1);
        std::memcpy(b + 173, &timeInForce, 1);
        std::memcpy(b + 174, &manualOrderIndicator, 1);
        std::memcpy(b + 175, &execInst, 1);
        std::memcpy(b + 176, &executionMode, 1);
        std::memcpy(b + 177, &liquidityFlag, 1);
        std::memcpy(b + 178, &managedOrder, 1);
        std::memcpy(b + 179, &shortSaleType, 1);
        std::memcpy(b + 180, &ordStatus, 1);
        std::memcpy(b + 181, &execType, 1);
        std::memcpy(b + 182, &expireDate, 2);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&seqNum, b + 0, 4);
        std::memcpy(&uuid, b + 4, 8);
        std::memcpy(execID, b + 12, 40);
        std::memcpy(senderID, b + 52, 20);
        std::memcpy(clOrdID, b + 72, 20);
        std::memcpy(&partyDetailsListReqID, b + 92, 8);
        std::memcpy(&orderID, b + 100, 8);
        std::memcpy(&price, b + 108, 8);
        std::memcpy(&stopPx, b + 116, 8);
        std::memcpy(&transactTime, b + 124, 8);
        std::memcpy(&sendingTimeEpoch, b + 132, 8);
        std::memcpy(&orderRequestID, b + 140, 8);
        std::memcpy(location, b + 148, 5);
        std::memcpy(&securityID, b + 153, 4);
        std::memcpy(&orderQty, b + 157, 4);
        std::memcpy(&minQty, b + 161, 4);
        std::memcpy(&displayQty, b + 165, 4);
        std::memcpy(&ordRejReason, b + 169, 2);
        std::memcpy(&ordType, b + 171, 1);
        std::memcpy(&side, b + 172, 1);
        std::memcpy(&timeInForce, b + 173, 1);
        std::memcpy(&manualOrderIndicator, b + 174, 1);
        std::memcpy(&execInst, b + 175, 1);
        std::memcpy(&executionMode, b + 176, 1);
        std::memcpy(&liquidityFlag, b + 177, 1);
        std::memcpy(&managedOrder, b + 178, 1);
        std::memcpy(&shortSaleType, b + 179, 1);
        std::memcpy(&ordStatus, b + 180, 1);
        std::memcpy(&execType, b + 181, 1);
        std::memcpy(&expireDate, b + 182, 2);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// ExecutionReportElimination (templateId=524)
// ============================================================================
struct ExecutionReportElimination524 {
    static constexpr uint16_t TEMPLATE_ID = 524;
    //   0  SeqNum(4)
    //   4  UUID(8)
    //  12  ExecID[40]
    //  52  SenderID[20]
    //  72  ClOrdID[20]
    //  92  PartyDetailsListReqID(8)
    // 100  OrderID(8)
    // 108  Price(8)
    // 116  StopPx(8)
    // 124  TransactTime(8)
    // 132  SendingTimeEpoch(8)
    // 140  OrderRequestID(8)
    // 148  Location[5]
    // 153  SecurityID(4)
    // 157  CumQty(4)
    // 161  OrderQty(4)
    // 165  MinQty(4)
    // 169  DisplayQty(4)
    // 173  OrdType(1)
    // 174  Side(1)
    // 175  TimeInForce(1)
    // 176  ManualOrderIndicator(1)
    // 177  ExecInst(1)
    // 178  ExecutionMode(1)
    // 179  LiquidityFlag(1)
    // 180  ManagedOrder(1)
    // 181  ShortSaleType(1)
    // 182  OrdStatus(1)
    // 183  ExecType(1)
    // 184  ExpireDate(2)
    static constexpr uint16_t BLOCK_LENGTH = 186;

    uint32_t seqNum = 0;
    uint64_t uuid = 0;
    char execID[40]{};
    char senderID[20]{};
    char clOrdID[20]{};
    uint64_t partyDetailsListReqID = 0;
    uint64_t orderID = 0;
    int64_t price = 0;
    int64_t stopPx = 0;
    uint64_t transactTime = 0;
    uint64_t sendingTimeEpoch = 0;
    uint64_t orderRequestID = 0;
    char location[5]{};
    int32_t securityID = 0;
    uint32_t cumQty = 0;
    uint32_t orderQty = 0;
    uint32_t minQty = 0;
    uint32_t displayQty = 0;
    uint8_t ordType = 0;
    uint8_t side = 0;
    uint8_t timeInForce = 0;
    uint8_t manualOrderIndicator = 0;
    char execInst = '\0';
    char executionMode = '\0';
    uint8_t liquidityFlag = 0;
    uint8_t managedOrder = 0;
    uint8_t shortSaleType = 0;
    char ordStatus = 'C';
    char execType = 'C';
    uint16_t expireDate = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,   &seqNum, 4);
        std::memcpy(b + 4,   &uuid, 8);
        std::memcpy(b + 12,  execID, 40);
        std::memcpy(b + 52,  senderID, 20);
        std::memcpy(b + 72,  clOrdID, 20);
        std::memcpy(b + 92,  &partyDetailsListReqID, 8);
        std::memcpy(b + 100, &orderID, 8);
        std::memcpy(b + 108, &price, 8);
        std::memcpy(b + 116, &stopPx, 8);
        std::memcpy(b + 124, &transactTime, 8);
        std::memcpy(b + 132, &sendingTimeEpoch, 8);
        std::memcpy(b + 140, &orderRequestID, 8);
        std::memcpy(b + 148, location, 5);
        std::memcpy(b + 153, &securityID, 4);
        std::memcpy(b + 157, &cumQty, 4);
        std::memcpy(b + 161, &orderQty, 4);
        std::memcpy(b + 165, &minQty, 4);
        std::memcpy(b + 169, &displayQty, 4);
        std::memcpy(b + 173, &ordType, 1);
        std::memcpy(b + 174, &side, 1);
        std::memcpy(b + 175, &timeInForce, 1);
        std::memcpy(b + 176, &manualOrderIndicator, 1);
        std::memcpy(b + 177, &execInst, 1);
        std::memcpy(b + 178, &executionMode, 1);
        std::memcpy(b + 179, &liquidityFlag, 1);
        std::memcpy(b + 180, &managedOrder, 1);
        std::memcpy(b + 181, &shortSaleType, 1);
        std::memcpy(b + 182, &ordStatus, 1);
        std::memcpy(b + 183, &execType, 1);
        std::memcpy(b + 184, &expireDate, 2);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&seqNum, b + 0, 4);
        std::memcpy(&uuid, b + 4, 8);
        std::memcpy(execID, b + 12, 40);
        std::memcpy(senderID, b + 52, 20);
        std::memcpy(clOrdID, b + 72, 20);
        std::memcpy(&partyDetailsListReqID, b + 92, 8);
        std::memcpy(&orderID, b + 100, 8);
        std::memcpy(&price, b + 108, 8);
        std::memcpy(&stopPx, b + 116, 8);
        std::memcpy(&transactTime, b + 124, 8);
        std::memcpy(&sendingTimeEpoch, b + 132, 8);
        std::memcpy(&orderRequestID, b + 140, 8);
        std::memcpy(location, b + 148, 5);
        std::memcpy(&securityID, b + 153, 4);
        std::memcpy(&cumQty, b + 157, 4);
        std::memcpy(&orderQty, b + 161, 4);
        std::memcpy(&minQty, b + 165, 4);
        std::memcpy(&displayQty, b + 169, 4);
        std::memcpy(&ordType, b + 173, 1);
        std::memcpy(&side, b + 174, 1);
        std::memcpy(&timeInForce, b + 175, 1);
        std::memcpy(&manualOrderIndicator, b + 176, 1);
        std::memcpy(&execInst, b + 177, 1);
        std::memcpy(&executionMode, b + 178, 1);
        std::memcpy(&liquidityFlag, b + 179, 1);
        std::memcpy(&managedOrder, b + 180, 1);
        std::memcpy(&shortSaleType, b + 181, 1);
        std::memcpy(&ordStatus, b + 182, 1);
        std::memcpy(&execType, b + 183, 1);
        std::memcpy(&expireDate, b + 184, 2);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// ExecutionReportTradeOutright (templateId=525)
// ============================================================================
struct ExecutionReportTradeOutright525 {
    static constexpr uint16_t TEMPLATE_ID = 525;
    //   0  SeqNum(4)
    //   4  UUID(8)
    //  12  ExecID[40]
    //  52  SenderID[20]
    //  72  ClOrdID[20]
    //  92  PartyDetailsListReqID(8)
    // 100  OrderID(8)
    // 108  Price(8)
    // 116  StopPx(8)
    // 124  TransactTime(8)
    // 132  SendingTimeEpoch(8)
    // 140  OrderRequestID(8)
    // 148  LastQty(4)
    // 152  LastPx(8)
    // 160  Location[5]
    // 165  SecurityID(4)
    // 169  OrderQty(4)
    // 173  CumQty(4)
    // 177  LeavesQty(4)
    // 181  MinQty(4)
    // 185  DisplayQty(4)
    // 189  SideTradeID(4)
    // 193  TradeDate(2)
    // 195  OrdType(1)
    // 196  Side(1)
    // 197  TimeInForce(1)
    // 198  ManualOrderIndicator(1)
    // 199  ExecInst(1)
    // 200  ExecutionMode(1)
    // 201  LiquidityFlag(1)
    // 202  ManagedOrder(1)
    // 203  ShortSaleType(1)
    // 204  OrdStatus(1)
    // 205  ExecType(1)
    // 206  AggressorIndicator(1)
    // 207  ExpireDate(2)
    // 209  FillPx(8)
    // 217  FillQty(4)
    static constexpr uint16_t BLOCK_LENGTH = 221;

    uint32_t seqNum = 0;
    uint64_t uuid = 0;
    char execID[40]{};
    char senderID[20]{};
    char clOrdID[20]{};
    uint64_t partyDetailsListReqID = 0;
    uint64_t orderID = 0;
    int64_t price = 0;
    int64_t stopPx = 0;
    uint64_t transactTime = 0;
    uint64_t sendingTimeEpoch = 0;
    uint64_t orderRequestID = 0;
    uint32_t lastQty = 0;
    int64_t lastPx = 0;
    char location[5]{};
    int32_t securityID = 0;
    uint32_t orderQty = 0;
    uint32_t cumQty = 0;
    uint32_t leavesQty = 0;
    uint32_t minQty = 0;
    uint32_t displayQty = 0;
    uint32_t sideTradeID = 0;
    uint16_t tradeDate = 0;
    uint8_t ordType = 0;
    uint8_t side = 0;
    uint8_t timeInForce = 0;
    uint8_t manualOrderIndicator = 0;
    char execInst = '\0';
    char executionMode = '\0';
    uint8_t liquidityFlag = 0;
    uint8_t managedOrder = 0;
    uint8_t shortSaleType = 0;
    char ordStatus = '0';
    char execType = 'F';
    uint8_t aggressorIndicator = 0;
    uint16_t expireDate = 0;
    int64_t fillPx = 0;
    uint32_t fillQty = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,   &seqNum, 4);
        std::memcpy(b + 4,   &uuid, 8);
        std::memcpy(b + 12,  execID, 40);
        std::memcpy(b + 52,  senderID, 20);
        std::memcpy(b + 72,  clOrdID, 20);
        std::memcpy(b + 92,  &partyDetailsListReqID, 8);
        std::memcpy(b + 100, &orderID, 8);
        std::memcpy(b + 108, &price, 8);
        std::memcpy(b + 116, &stopPx, 8);
        std::memcpy(b + 124, &transactTime, 8);
        std::memcpy(b + 132, &sendingTimeEpoch, 8);
        std::memcpy(b + 140, &orderRequestID, 8);
        std::memcpy(b + 148, &lastQty, 4);
        std::memcpy(b + 152, &lastPx, 8);
        std::memcpy(b + 160, location, 5);
        std::memcpy(b + 165, &securityID, 4);
        std::memcpy(b + 169, &orderQty, 4);
        std::memcpy(b + 173, &cumQty, 4);
        std::memcpy(b + 177, &leavesQty, 4);
        std::memcpy(b + 181, &minQty, 4);
        std::memcpy(b + 185, &displayQty, 4);
        std::memcpy(b + 189, &sideTradeID, 4);
        std::memcpy(b + 193, &tradeDate, 2);
        std::memcpy(b + 195, &ordType, 1);
        std::memcpy(b + 196, &side, 1);
        std::memcpy(b + 197, &timeInForce, 1);
        std::memcpy(b + 198, &manualOrderIndicator, 1);
        std::memcpy(b + 199, &execInst, 1);
        std::memcpy(b + 200, &executionMode, 1);
        std::memcpy(b + 201, &liquidityFlag, 1);
        std::memcpy(b + 202, &managedOrder, 1);
        std::memcpy(b + 203, &shortSaleType, 1);
        std::memcpy(b + 204, &ordStatus, 1);
        std::memcpy(b + 205, &execType, 1);
        std::memcpy(b + 206, &aggressorIndicator, 1);
        std::memcpy(b + 207, &expireDate, 2);
        std::memcpy(b + 209, &fillPx, 8);
        std::memcpy(b + 217, &fillQty, 4);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&seqNum, b + 0, 4);
        std::memcpy(&uuid, b + 4, 8);
        std::memcpy(execID, b + 12, 40);
        std::memcpy(senderID, b + 52, 20);
        std::memcpy(clOrdID, b + 72, 20);
        std::memcpy(&partyDetailsListReqID, b + 92, 8);
        std::memcpy(&orderID, b + 100, 8);
        std::memcpy(&price, b + 108, 8);
        std::memcpy(&stopPx, b + 116, 8);
        std::memcpy(&transactTime, b + 124, 8);
        std::memcpy(&sendingTimeEpoch, b + 132, 8);
        std::memcpy(&orderRequestID, b + 140, 8);
        std::memcpy(&lastQty, b + 148, 4);
        std::memcpy(&lastPx, b + 152, 8);
        std::memcpy(location, b + 160, 5);
        std::memcpy(&securityID, b + 165, 4);
        std::memcpy(&orderQty, b + 169, 4);
        std::memcpy(&cumQty, b + 173, 4);
        std::memcpy(&leavesQty, b + 177, 4);
        std::memcpy(&minQty, b + 181, 4);
        std::memcpy(&displayQty, b + 185, 4);
        std::memcpy(&sideTradeID, b + 189, 4);
        std::memcpy(&tradeDate, b + 193, 2);
        std::memcpy(&ordType, b + 195, 1);
        std::memcpy(&side, b + 196, 1);
        std::memcpy(&timeInForce, b + 197, 1);
        std::memcpy(&manualOrderIndicator, b + 198, 1);
        std::memcpy(&execInst, b + 199, 1);
        std::memcpy(&executionMode, b + 200, 1);
        std::memcpy(&liquidityFlag, b + 201, 1);
        std::memcpy(&managedOrder, b + 202, 1);
        std::memcpy(&shortSaleType, b + 203, 1);
        std::memcpy(&ordStatus, b + 204, 1);
        std::memcpy(&execType, b + 205, 1);
        std::memcpy(&aggressorIndicator, b + 206, 1);
        std::memcpy(&expireDate, b + 207, 2);
        std::memcpy(&fillPx, b + 209, 8);
        std::memcpy(&fillQty, b + 217, 4);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// ExecutionReportCancel (templateId=534)
// ============================================================================
struct ExecutionReportCancel534 {
    static constexpr uint16_t TEMPLATE_ID = 534;
    //   0  SeqNum(4)
    //   4  UUID(8)
    //  12  ExecID[40]
    //  52  SenderID[20]
    //  72  ClOrdID[20]
    //  92  PartyDetailsListReqID(8)
    // 100  OrderID(8)
    // 108  Price(8)
    // 116  StopPx(8)
    // 124  TransactTime(8)
    // 132  SendingTimeEpoch(8)
    // 140  OrderRequestID(8)
    // 148  Location[5]
    // 153  SecurityID(4)
    // 157  CumQty(4)
    // 161  OrderQty(4)
    // 165  MinQty(4)
    // 169  DisplayQty(4)
    // 173  OrdType(1)
    // 174  Side(1)
    // 175  TimeInForce(1)
    // 176  ManualOrderIndicator(1)
    // 177  ExecInst(1)
    // 178  ExecutionMode(1)
    // 179  LiquidityFlag(1)
    // 180  ManagedOrder(1)
    // 181  ShortSaleType(1)
    // 182  OrdStatus(1)
    // 183  ExecType(1)
    // 184  ExpireDate(2)
    static constexpr uint16_t BLOCK_LENGTH = 186;

    uint32_t seqNum = 0;
    uint64_t uuid = 0;
    char execID[40]{};
    char senderID[20]{};
    char clOrdID[20]{};
    uint64_t partyDetailsListReqID = 0;
    uint64_t orderID = 0;
    int64_t price = 0;
    int64_t stopPx = 0;
    uint64_t transactTime = 0;
    uint64_t sendingTimeEpoch = 0;
    uint64_t orderRequestID = 0;
    char location[5]{};
    int32_t securityID = 0;
    uint32_t cumQty = 0;
    uint32_t orderQty = 0;
    uint32_t minQty = 0;
    uint32_t displayQty = 0;
    uint8_t ordType = 0;
    uint8_t side = 0;
    uint8_t timeInForce = 0;
    uint8_t manualOrderIndicator = 0;
    char execInst = '\0';
    char executionMode = '\0';
    uint8_t liquidityFlag = 0;
    uint8_t managedOrder = 0;
    uint8_t shortSaleType = 0;
    char ordStatus = '4';
    char execType = '4';
    uint16_t expireDate = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,   &seqNum, 4);
        std::memcpy(b + 4,   &uuid, 8);
        std::memcpy(b + 12,  execID, 40);
        std::memcpy(b + 52,  senderID, 20);
        std::memcpy(b + 72,  clOrdID, 20);
        std::memcpy(b + 92,  &partyDetailsListReqID, 8);
        std::memcpy(b + 100, &orderID, 8);
        std::memcpy(b + 108, &price, 8);
        std::memcpy(b + 116, &stopPx, 8);
        std::memcpy(b + 124, &transactTime, 8);
        std::memcpy(b + 132, &sendingTimeEpoch, 8);
        std::memcpy(b + 140, &orderRequestID, 8);
        std::memcpy(b + 148, location, 5);
        std::memcpy(b + 153, &securityID, 4);
        std::memcpy(b + 157, &cumQty, 4);
        std::memcpy(b + 161, &orderQty, 4);
        std::memcpy(b + 165, &minQty, 4);
        std::memcpy(b + 169, &displayQty, 4);
        std::memcpy(b + 173, &ordType, 1);
        std::memcpy(b + 174, &side, 1);
        std::memcpy(b + 175, &timeInForce, 1);
        std::memcpy(b + 176, &manualOrderIndicator, 1);
        std::memcpy(b + 177, &execInst, 1);
        std::memcpy(b + 178, &executionMode, 1);
        std::memcpy(b + 179, &liquidityFlag, 1);
        std::memcpy(b + 180, &managedOrder, 1);
        std::memcpy(b + 181, &shortSaleType, 1);
        std::memcpy(b + 182, &ordStatus, 1);
        std::memcpy(b + 183, &execType, 1);
        std::memcpy(b + 184, &expireDate, 2);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&seqNum, b + 0, 4);
        std::memcpy(&uuid, b + 4, 8);
        std::memcpy(execID, b + 12, 40);
        std::memcpy(senderID, b + 52, 20);
        std::memcpy(clOrdID, b + 72, 20);
        std::memcpy(&partyDetailsListReqID, b + 92, 8);
        std::memcpy(&orderID, b + 100, 8);
        std::memcpy(&price, b + 108, 8);
        std::memcpy(&stopPx, b + 116, 8);
        std::memcpy(&transactTime, b + 124, 8);
        std::memcpy(&sendingTimeEpoch, b + 132, 8);
        std::memcpy(&orderRequestID, b + 140, 8);
        std::memcpy(location, b + 148, 5);
        std::memcpy(&securityID, b + 153, 4);
        std::memcpy(&cumQty, b + 157, 4);
        std::memcpy(&orderQty, b + 161, 4);
        std::memcpy(&minQty, b + 165, 4);
        std::memcpy(&displayQty, b + 169, 4);
        std::memcpy(&ordType, b + 173, 1);
        std::memcpy(&side, b + 174, 1);
        std::memcpy(&timeInForce, b + 175, 1);
        std::memcpy(&manualOrderIndicator, b + 176, 1);
        std::memcpy(&execInst, b + 177, 1);
        std::memcpy(&executionMode, b + 178, 1);
        std::memcpy(&liquidityFlag, b + 179, 1);
        std::memcpy(&managedOrder, b + 180, 1);
        std::memcpy(&shortSaleType, b + 181, 1);
        std::memcpy(&ordStatus, b + 182, 1);
        std::memcpy(&execType, b + 183, 1);
        std::memcpy(&expireDate, b + 184, 2);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// ExecutionReportModify (templateId=531)
// ============================================================================
struct ExecutionReportModify531 {
    static constexpr uint16_t TEMPLATE_ID = 531;
    // Same layout as Cancel534
    static constexpr uint16_t BLOCK_LENGTH = 186;

    uint32_t seqNum = 0;
    uint64_t uuid = 0;
    char execID[40]{};
    char senderID[20]{};
    char clOrdID[20]{};
    uint64_t partyDetailsListReqID = 0;
    uint64_t orderID = 0;
    int64_t price = 0;
    int64_t stopPx = 0;
    uint64_t transactTime = 0;
    uint64_t sendingTimeEpoch = 0;
    uint64_t orderRequestID = 0;
    char location[5]{};
    int32_t securityID = 0;
    uint32_t cumQty = 0;
    uint32_t orderQty = 0;
    uint32_t minQty = 0;
    uint32_t displayQty = 0;
    uint8_t ordType = 0;
    uint8_t side = 0;
    uint8_t timeInForce = 0;
    uint8_t manualOrderIndicator = 0;
    char execInst = '\0';
    char executionMode = '\0';
    uint8_t liquidityFlag = 0;
    uint8_t managedOrder = 0;
    uint8_t shortSaleType = 0;
    char ordStatus = '0';
    char execType = '5';
    uint16_t expireDate = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,   &seqNum, 4);
        std::memcpy(b + 4,   &uuid, 8);
        std::memcpy(b + 12,  execID, 40);
        std::memcpy(b + 52,  senderID, 20);
        std::memcpy(b + 72,  clOrdID, 20);
        std::memcpy(b + 92,  &partyDetailsListReqID, 8);
        std::memcpy(b + 100, &orderID, 8);
        std::memcpy(b + 108, &price, 8);
        std::memcpy(b + 116, &stopPx, 8);
        std::memcpy(b + 124, &transactTime, 8);
        std::memcpy(b + 132, &sendingTimeEpoch, 8);
        std::memcpy(b + 140, &orderRequestID, 8);
        std::memcpy(b + 148, location, 5);
        std::memcpy(b + 153, &securityID, 4);
        std::memcpy(b + 157, &cumQty, 4);
        std::memcpy(b + 161, &orderQty, 4);
        std::memcpy(b + 165, &minQty, 4);
        std::memcpy(b + 169, &displayQty, 4);
        std::memcpy(b + 173, &ordType, 1);
        std::memcpy(b + 174, &side, 1);
        std::memcpy(b + 175, &timeInForce, 1);
        std::memcpy(b + 176, &manualOrderIndicator, 1);
        std::memcpy(b + 177, &execInst, 1);
        std::memcpy(b + 178, &executionMode, 1);
        std::memcpy(b + 179, &liquidityFlag, 1);
        std::memcpy(b + 180, &managedOrder, 1);
        std::memcpy(b + 181, &shortSaleType, 1);
        std::memcpy(b + 182, &ordStatus, 1);
        std::memcpy(b + 183, &execType, 1);
        std::memcpy(b + 184, &expireDate, 2);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&seqNum, b + 0, 4);
        std::memcpy(&uuid, b + 4, 8);
        std::memcpy(execID, b + 12, 40);
        std::memcpy(senderID, b + 52, 20);
        std::memcpy(clOrdID, b + 72, 20);
        std::memcpy(&partyDetailsListReqID, b + 92, 8);
        std::memcpy(&orderID, b + 100, 8);
        std::memcpy(&price, b + 108, 8);
        std::memcpy(&stopPx, b + 116, 8);
        std::memcpy(&transactTime, b + 124, 8);
        std::memcpy(&sendingTimeEpoch, b + 132, 8);
        std::memcpy(&orderRequestID, b + 140, 8);
        std::memcpy(location, b + 148, 5);
        std::memcpy(&securityID, b + 153, 4);
        std::memcpy(&cumQty, b + 157, 4);
        std::memcpy(&orderQty, b + 161, 4);
        std::memcpy(&minQty, b + 165, 4);
        std::memcpy(&displayQty, b + 169, 4);
        std::memcpy(&ordType, b + 173, 1);
        std::memcpy(&side, b + 174, 1);
        std::memcpy(&timeInForce, b + 175, 1);
        std::memcpy(&manualOrderIndicator, b + 176, 1);
        std::memcpy(&execInst, b + 177, 1);
        std::memcpy(&executionMode, b + 178, 1);
        std::memcpy(&liquidityFlag, b + 179, 1);
        std::memcpy(&managedOrder, b + 180, 1);
        std::memcpy(&shortSaleType, b + 181, 1);
        std::memcpy(&ordStatus, b + 182, 1);
        std::memcpy(&execType, b + 183, 1);
        std::memcpy(&expireDate, b + 184, 2);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// OrderCancelReject (templateId=535)
// ============================================================================
struct OrderCancelReject535 {
    static constexpr uint16_t TEMPLATE_ID = 535;
    //   0  SeqNum(4)
    //   4  UUID(8)
    //  12  ExecID[40]
    //  52  SenderID[20]
    //  72  ClOrdID[20]
    //  92  PartyDetailsListReqID(8)
    // 100  OrderID(8)
    // 108  TransactTime(8)
    // 116  SendingTimeEpoch(8)
    // 124  OrderRequestID(8)
    // 132  Location[5]
    // 137  CxlRejReason(2)
    // 139  ManualOrderIndicator(1)
    static constexpr uint16_t BLOCK_LENGTH = 140;

    uint32_t seqNum = 0;
    uint64_t uuid = 0;
    char execID[40]{};
    char senderID[20]{};
    char clOrdID[20]{};
    uint64_t partyDetailsListReqID = 0;
    uint64_t orderID = 0;
    uint64_t transactTime = 0;
    uint64_t sendingTimeEpoch = 0;
    uint64_t orderRequestID = 0;
    char location[5]{};
    uint16_t cxlRejReason = 0;
    uint8_t manualOrderIndicator = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeILink3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,   &seqNum, 4);
        std::memcpy(b + 4,   &uuid, 8);
        std::memcpy(b + 12,  execID, 40);
        std::memcpy(b + 52,  senderID, 20);
        std::memcpy(b + 72,  clOrdID, 20);
        std::memcpy(b + 92,  &partyDetailsListReqID, 8);
        std::memcpy(b + 100, &orderID, 8);
        std::memcpy(b + 108, &transactTime, 8);
        std::memcpy(b + 116, &sendingTimeEpoch, 8);
        std::memcpy(b + 124, &orderRequestID, 8);
        std::memcpy(b + 132, location, 5);
        std::memcpy(b + 137, &cxlRejReason, 2);
        std::memcpy(b + 139, &manualOrderIndicator, 1);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&seqNum, b + 0, 4);
        std::memcpy(&uuid, b + 4, 8);
        std::memcpy(execID, b + 12, 40);
        std::memcpy(senderID, b + 52, 20);
        std::memcpy(clOrdID, b + 72, 20);
        std::memcpy(&partyDetailsListReqID, b + 92, 8);
        std::memcpy(&orderID, b + 100, 8);
        std::memcpy(&transactTime, b + 108, 8);
        std::memcpy(&sendingTimeEpoch, b + 116, 8);
        std::memcpy(&orderRequestID, b + 124, 8);
        std::memcpy(location, b + 132, 5);
        std::memcpy(&cxlRejReason, b + 137, 2);
        std::memcpy(&manualOrderIndicator, b + 139, 1);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

} // namespace cme::sim::sbe
