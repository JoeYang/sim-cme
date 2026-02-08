#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include "message_header.h"

namespace cme::sim::sbe {

// MDP 3.0 Group Size Encoding: 2B blockLength + 1B numInGroup (per CME convention)
struct GroupSize {
    static constexpr size_t SIZE = 3;

    static void encode(char* buffer, uint16_t blockLength, uint8_t numInGroup) {
        std::memcpy(buffer, &blockLength, 2);
        std::memcpy(buffer + 2, &numInGroup, 1);
    }

    static uint16_t decodeBlockLength(const char* buffer) {
        uint16_t val;
        std::memcpy(&val, buffer, 2);
        return val;
    }

    static uint8_t decodeNumInGroup(const char* buffer) {
        uint8_t val;
        std::memcpy(&val, buffer + 2, 1);
        return val;
    }
};

// ============================================================================
// ChannelReset4 (templateId=4)
// ============================================================================
struct ChannelReset4 {
    static constexpr uint16_t TEMPLATE_ID = 4;
    //   0  TransactTime(8)
    //   8  MatchEventIndicator(1)
    static constexpr uint16_t BLOCK_LENGTH = 9;

    struct Entry {
        static constexpr uint16_t ENTRY_BLOCK_LENGTH = 2;
        int16_t applID = 0;
    };

    uint64_t transactTime = 0;
    uint8_t matchEventIndicator = 0;
    std::vector<Entry> entries;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeMDP3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0, &transactTime, 8);
        std::memcpy(b + 8, &matchEventIndicator, 1);
        size_t pos = BLOCK_LENGTH;
        GroupSize::encode(b + pos, Entry::ENTRY_BLOCK_LENGTH,
                          static_cast<uint8_t>(entries.size()));
        pos += GroupSize::SIZE;
        for (const auto& e : entries) {
            std::memcpy(b + pos, &e.applID, 2);
            pos += Entry::ENTRY_BLOCK_LENGTH;
        }
        return MessageHeader::SIZE + pos;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&transactTime, b + 0, 8);
        std::memcpy(&matchEventIndicator, b + 8, 1);
        size_t pos = BLOCK_LENGTH;
        uint16_t entryBlockLen = GroupSize::decodeBlockLength(b + pos);
        uint8_t numEntries = GroupSize::decodeNumInGroup(b + pos);
        pos += GroupSize::SIZE;
        entries.resize(numEntries);
        for (uint8_t i = 0; i < numEntries; ++i) {
            std::memcpy(&entries[i].applID, b + pos, 2);
            pos += entryBlockLen;
        }
    }

    size_t encodedLength() const {
        return MessageHeader::SIZE + BLOCK_LENGTH + GroupSize::SIZE +
               entries.size() * Entry::ENTRY_BLOCK_LENGTH;
    }
};

// ============================================================================
// AdminHeartbeat12 (templateId=12)
// ============================================================================
struct AdminHeartbeat12 {
    static constexpr uint16_t TEMPLATE_ID = 12;
    static constexpr uint16_t BLOCK_LENGTH = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeMDP3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* /*buffer*/, size_t /*offset*/) {
        // No body fields
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// SecurityStatus30 (templateId=30)
// ============================================================================
struct SecurityStatus30 {
    static constexpr uint16_t TEMPLATE_ID = 30;
    //   0  TransactTime(8)
    //   8  SecurityGroup[6]
    //  14  Asset[6]
    //  20  SecurityID(4)
    //  24  TradeDate(2)
    //  26  MatchEventIndicator(1)
    //  27  SecurityTradingStatus(1)
    //  28  HaltReason(1)
    //  29  SecurityTradingEvent(1)
    static constexpr uint16_t BLOCK_LENGTH = 30;

    uint64_t transactTime = 0;
    char securityGroup[6]{};
    char asset[6]{};
    int32_t securityID = 0;
    uint16_t tradeDate = 0;
    uint8_t matchEventIndicator = 0;
    uint8_t securityTradingStatus = 0;
    uint8_t haltReason = 0;
    uint8_t securityTradingEvent = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeMDP3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,  &transactTime, 8);
        std::memcpy(b + 8,  securityGroup, 6);
        std::memcpy(b + 14, asset, 6);
        std::memcpy(b + 20, &securityID, 4);
        std::memcpy(b + 24, &tradeDate, 2);
        std::memcpy(b + 26, &matchEventIndicator, 1);
        std::memcpy(b + 27, &securityTradingStatus, 1);
        std::memcpy(b + 28, &haltReason, 1);
        std::memcpy(b + 29, &securityTradingEvent, 1);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&transactTime, b + 0, 8);
        std::memcpy(securityGroup, b + 8, 6);
        std::memcpy(asset, b + 14, 6);
        std::memcpy(&securityID, b + 20, 4);
        std::memcpy(&tradeDate, b + 24, 2);
        std::memcpy(&matchEventIndicator, b + 26, 1);
        std::memcpy(&securityTradingStatus, b + 27, 1);
        std::memcpy(&haltReason, b + 28, 1);
        std::memcpy(&securityTradingEvent, b + 29, 1);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

// ============================================================================
// MDIncrementalRefreshBook46 (templateId=46)
// ============================================================================
struct MDIncrementalRefreshBook46 {
    static constexpr uint16_t TEMPLATE_ID = 46;
    //   0  TransactTime(8)
    //   8  MatchEventIndicator(1)
    static constexpr uint16_t BLOCK_LENGTH = 9;

    struct Entry {
        //   0  MDEntryPx(8)
        //   8  MDEntrySize(4)
        //  12  SecurityID(4)
        //  16  RptSeq(4)
        //  20  NumberOfOrders(4)
        //  24  MDPriceLevel(1)
        //  25  MDUpdateAction(1)
        //  26  MDEntryType(1)
        static constexpr uint16_t ENTRY_BLOCK_LENGTH = 27;

        int64_t mdEntryPx = 0;
        int32_t mdEntrySize = 0;
        int32_t securityID = 0;
        uint32_t rptSeq = 0;
        int32_t numberOfOrders = 0;
        uint8_t mdPriceLevel = 0;
        uint8_t mdUpdateAction = 0;
        char mdEntryType = '\0';
    };

    uint64_t transactTime = 0;
    uint8_t matchEventIndicator = 0;
    std::vector<Entry> entries;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeMDP3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0, &transactTime, 8);
        std::memcpy(b + 8, &matchEventIndicator, 1);
        size_t pos = BLOCK_LENGTH;
        GroupSize::encode(b + pos, Entry::ENTRY_BLOCK_LENGTH,
                          static_cast<uint8_t>(entries.size()));
        pos += GroupSize::SIZE;
        for (const auto& e : entries) {
            std::memcpy(b + pos + 0,  &e.mdEntryPx, 8);
            std::memcpy(b + pos + 8,  &e.mdEntrySize, 4);
            std::memcpy(b + pos + 12, &e.securityID, 4);
            std::memcpy(b + pos + 16, &e.rptSeq, 4);
            std::memcpy(b + pos + 20, &e.numberOfOrders, 4);
            std::memcpy(b + pos + 24, &e.mdPriceLevel, 1);
            std::memcpy(b + pos + 25, &e.mdUpdateAction, 1);
            std::memcpy(b + pos + 26, &e.mdEntryType, 1);
            pos += Entry::ENTRY_BLOCK_LENGTH;
        }
        return MessageHeader::SIZE + pos;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&transactTime, b + 0, 8);
        std::memcpy(&matchEventIndicator, b + 8, 1);
        size_t pos = BLOCK_LENGTH;
        uint16_t entryBlockLen = GroupSize::decodeBlockLength(b + pos);
        uint8_t numEntries = GroupSize::decodeNumInGroup(b + pos);
        pos += GroupSize::SIZE;
        entries.resize(numEntries);
        for (uint8_t i = 0; i < numEntries; ++i) {
            std::memcpy(&entries[i].mdEntryPx, b + pos + 0, 8);
            std::memcpy(&entries[i].mdEntrySize, b + pos + 8, 4);
            std::memcpy(&entries[i].securityID, b + pos + 12, 4);
            std::memcpy(&entries[i].rptSeq, b + pos + 16, 4);
            std::memcpy(&entries[i].numberOfOrders, b + pos + 20, 4);
            std::memcpy(&entries[i].mdPriceLevel, b + pos + 24, 1);
            std::memcpy(&entries[i].mdUpdateAction, b + pos + 25, 1);
            std::memcpy(&entries[i].mdEntryType, b + pos + 26, 1);
            pos += entryBlockLen;
        }
    }

    size_t encodedLength() const {
        return MessageHeader::SIZE + BLOCK_LENGTH + GroupSize::SIZE +
               entries.size() * Entry::ENTRY_BLOCK_LENGTH;
    }
};

// ============================================================================
// MDIncrementalRefreshTradeSummary48 (templateId=48)
// ============================================================================
struct MDIncrementalRefreshTradeSummary48 {
    static constexpr uint16_t TEMPLATE_ID = 48;
    static constexpr uint16_t BLOCK_LENGTH = 9;

    struct MDEntry {
        //   0  MDEntryPx(8)
        //   8  MDEntrySize(4)
        //  12  SecurityID(4)
        //  16  RptSeq(4)
        //  20  NumberOfOrders(4)
        //  24  AggressorSide(1)
        //  25  MDUpdateAction(1)
        static constexpr uint16_t ENTRY_BLOCK_LENGTH = 26;

        int64_t mdEntryPx = 0;
        int32_t mdEntrySize = 0;
        int32_t securityID = 0;
        uint32_t rptSeq = 0;
        int32_t numberOfOrders = 0;
        uint8_t aggressorSide = 0;
        uint8_t mdUpdateAction = 0;
    };

    struct OrderIDEntry {
        //   0  OrderID(8)
        //   8  LastQty(4)
        static constexpr uint16_t ENTRY_BLOCK_LENGTH = 12;

        uint64_t orderID = 0;
        int32_t lastQty = 0;
    };

    uint64_t transactTime = 0;
    uint8_t matchEventIndicator = 0;
    std::vector<MDEntry> mdEntries;
    std::vector<OrderIDEntry> orderIDEntries;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeMDP3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0, &transactTime, 8);
        std::memcpy(b + 8, &matchEventIndicator, 1);
        size_t pos = BLOCK_LENGTH;

        // Group 1: NoMDEntries
        GroupSize::encode(b + pos, MDEntry::ENTRY_BLOCK_LENGTH,
                          static_cast<uint8_t>(mdEntries.size()));
        pos += GroupSize::SIZE;
        for (const auto& e : mdEntries) {
            std::memcpy(b + pos + 0,  &e.mdEntryPx, 8);
            std::memcpy(b + pos + 8,  &e.mdEntrySize, 4);
            std::memcpy(b + pos + 12, &e.securityID, 4);
            std::memcpy(b + pos + 16, &e.rptSeq, 4);
            std::memcpy(b + pos + 20, &e.numberOfOrders, 4);
            std::memcpy(b + pos + 24, &e.aggressorSide, 1);
            std::memcpy(b + pos + 25, &e.mdUpdateAction, 1);
            pos += MDEntry::ENTRY_BLOCK_LENGTH;
        }

        // Group 2: NoOrderIDEntries
        GroupSize::encode(b + pos, OrderIDEntry::ENTRY_BLOCK_LENGTH,
                          static_cast<uint8_t>(orderIDEntries.size()));
        pos += GroupSize::SIZE;
        for (const auto& e : orderIDEntries) {
            std::memcpy(b + pos + 0, &e.orderID, 8);
            std::memcpy(b + pos + 8, &e.lastQty, 4);
            pos += OrderIDEntry::ENTRY_BLOCK_LENGTH;
        }
        return MessageHeader::SIZE + pos;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&transactTime, b + 0, 8);
        std::memcpy(&matchEventIndicator, b + 8, 1);
        size_t pos = BLOCK_LENGTH;

        // Group 1
        uint16_t mdBlockLen = GroupSize::decodeBlockLength(b + pos);
        uint8_t numMD = GroupSize::decodeNumInGroup(b + pos);
        pos += GroupSize::SIZE;
        mdEntries.resize(numMD);
        for (uint8_t i = 0; i < numMD; ++i) {
            std::memcpy(&mdEntries[i].mdEntryPx, b + pos + 0, 8);
            std::memcpy(&mdEntries[i].mdEntrySize, b + pos + 8, 4);
            std::memcpy(&mdEntries[i].securityID, b + pos + 12, 4);
            std::memcpy(&mdEntries[i].rptSeq, b + pos + 16, 4);
            std::memcpy(&mdEntries[i].numberOfOrders, b + pos + 20, 4);
            std::memcpy(&mdEntries[i].aggressorSide, b + pos + 24, 1);
            std::memcpy(&mdEntries[i].mdUpdateAction, b + pos + 25, 1);
            pos += mdBlockLen;
        }

        // Group 2
        uint16_t oidBlockLen = GroupSize::decodeBlockLength(b + pos);
        uint8_t numOID = GroupSize::decodeNumInGroup(b + pos);
        pos += GroupSize::SIZE;
        orderIDEntries.resize(numOID);
        for (uint8_t i = 0; i < numOID; ++i) {
            std::memcpy(&orderIDEntries[i].orderID, b + pos + 0, 8);
            std::memcpy(&orderIDEntries[i].lastQty, b + pos + 8, 4);
            pos += oidBlockLen;
        }
    }

    size_t encodedLength() const {
        return MessageHeader::SIZE + BLOCK_LENGTH +
               GroupSize::SIZE + mdEntries.size() * MDEntry::ENTRY_BLOCK_LENGTH +
               GroupSize::SIZE + orderIDEntries.size() * OrderIDEntry::ENTRY_BLOCK_LENGTH;
    }
};

// ============================================================================
// SnapshotFullRefresh52 (templateId=52)
// ============================================================================
struct SnapshotFullRefresh52 {
    static constexpr uint16_t TEMPLATE_ID = 52;
    //   0  LastMsgSeqNumProcessed(4)
    //   4  TotNumReports(4)
    //   8  SecurityID(4)
    //  12  RptSeq(4)
    //  16  TransactTime(8)
    //  24  LastUpdateTime(8)
    //  32  TradeDate(2)
    //  34  MDSecurityTradingStatus(1)
    //  35  HighLimitPrice(8)
    //  43  LowLimitPrice(8)
    //  51  MaxPriceVariation(8)
    static constexpr uint16_t BLOCK_LENGTH = 59;

    struct Entry {
        //   0  MDEntryPx(8)
        //   8  MDEntrySize(4)
        //  12  NumberOfOrders(4)
        //  16  MDPriceLevel(1)
        //  17  MDEntryType(1)
        static constexpr uint16_t ENTRY_BLOCK_LENGTH = 18;

        int64_t mdEntryPx = 0;
        int32_t mdEntrySize = 0;
        int32_t numberOfOrders = 0;
        uint8_t mdPriceLevel = 0;
        char mdEntryType = '\0';
    };

    uint32_t lastMsgSeqNumProcessed = 0;
    uint32_t totNumReports = 0;
    int32_t securityID = 0;
    uint32_t rptSeq = 0;
    uint64_t transactTime = 0;
    uint64_t lastUpdateTime = 0;
    uint16_t tradeDate = 0;
    uint8_t mdSecurityTradingStatus = 0;
    int64_t highLimitPrice = 0;
    int64_t lowLimitPrice = 0;
    int64_t maxPriceVariation = 0;
    std::vector<Entry> entries;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeMDP3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,  &lastMsgSeqNumProcessed, 4);
        std::memcpy(b + 4,  &totNumReports, 4);
        std::memcpy(b + 8,  &securityID, 4);
        std::memcpy(b + 12, &rptSeq, 4);
        std::memcpy(b + 16, &transactTime, 8);
        std::memcpy(b + 24, &lastUpdateTime, 8);
        std::memcpy(b + 32, &tradeDate, 2);
        std::memcpy(b + 34, &mdSecurityTradingStatus, 1);
        std::memcpy(b + 35, &highLimitPrice, 8);
        std::memcpy(b + 43, &lowLimitPrice, 8);
        std::memcpy(b + 51, &maxPriceVariation, 8);
        size_t pos = BLOCK_LENGTH;
        GroupSize::encode(b + pos, Entry::ENTRY_BLOCK_LENGTH,
                          static_cast<uint8_t>(entries.size()));
        pos += GroupSize::SIZE;
        for (const auto& e : entries) {
            std::memcpy(b + pos + 0,  &e.mdEntryPx, 8);
            std::memcpy(b + pos + 8,  &e.mdEntrySize, 4);
            std::memcpy(b + pos + 12, &e.numberOfOrders, 4);
            std::memcpy(b + pos + 16, &e.mdPriceLevel, 1);
            std::memcpy(b + pos + 17, &e.mdEntryType, 1);
            pos += Entry::ENTRY_BLOCK_LENGTH;
        }
        return MessageHeader::SIZE + pos;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&lastMsgSeqNumProcessed, b + 0, 4);
        std::memcpy(&totNumReports, b + 4, 4);
        std::memcpy(&securityID, b + 8, 4);
        std::memcpy(&rptSeq, b + 12, 4);
        std::memcpy(&transactTime, b + 16, 8);
        std::memcpy(&lastUpdateTime, b + 24, 8);
        std::memcpy(&tradeDate, b + 32, 2);
        std::memcpy(&mdSecurityTradingStatus, b + 34, 1);
        std::memcpy(&highLimitPrice, b + 35, 8);
        std::memcpy(&lowLimitPrice, b + 43, 8);
        std::memcpy(&maxPriceVariation, b + 51, 8);
        size_t pos = BLOCK_LENGTH;
        uint16_t entryBlockLen = GroupSize::decodeBlockLength(b + pos);
        uint8_t numEntries = GroupSize::decodeNumInGroup(b + pos);
        pos += GroupSize::SIZE;
        entries.resize(numEntries);
        for (uint8_t i = 0; i < numEntries; ++i) {
            std::memcpy(&entries[i].mdEntryPx, b + pos + 0, 8);
            std::memcpy(&entries[i].mdEntrySize, b + pos + 8, 4);
            std::memcpy(&entries[i].numberOfOrders, b + pos + 12, 4);
            std::memcpy(&entries[i].mdPriceLevel, b + pos + 16, 1);
            std::memcpy(&entries[i].mdEntryType, b + pos + 17, 1);
            pos += entryBlockLen;
        }
    }

    size_t encodedLength() const {
        return MessageHeader::SIZE + BLOCK_LENGTH + GroupSize::SIZE +
               entries.size() * Entry::ENTRY_BLOCK_LENGTH;
    }
};

// ============================================================================
// MDInstrumentDefinitionFuture54 (templateId=54)
// ============================================================================
struct MDInstrumentDefinitionFuture54 {
    static constexpr uint16_t TEMPLATE_ID = 54;
    //   0  MatchEventIndicator(1)
    //   1  TotNumReports(4)
    //   5  SecurityUpdateAction(1)
    //   6  LastUpdateTime(8)
    //  14  MDSecurityTradingStatus(1)
    //  15  ApplID(2)
    //  17  MarketSegmentID(1)
    //  18  UnderlyingProduct(1)
    //  19  SecurityExchange[4]
    //  23  SecurityGroup[6]
    //  29  Asset[6]
    //  35  Symbol[20]
    //  55  SecurityID(4)
    //  59  SecurityType[6]
    //  65  CFICode[6]
    //  71  MaturityMonthYear[5]  (YYYYMM as uint16+uint8+uint8+uint8)
    //  76  Currency[3]
    //  79  SettlCurrency[3]
    //  82  MatchAlgorithm(1)
    //  83  MinTradeVol(4)
    //  87  MaxTradeVol(4)
    //  91  MinPriceIncrement(8)
    //  99  DisplayFactor(8)
    // 107  MainFraction(1)
    // 108  SubFraction(1)
    // 109  PriceDisplayFormat(1)
    // 110  UnitOfMeasure[30]
    // 140  UnitOfMeasureQty(8)
    // 148  TradingReferencePrice(8)
    // 156  SettlPriceType(1)
    // 157  OpenInterestQty(4)
    // 161  ClearedVolume(4)
    // 165  HighLimitPrice(8)
    // 173  LowLimitPrice(8)
    // 181  MaxPriceVariation(8)
    // 189  DecayQuantity(4)
    // 193  DecayStartDate(2)
    // 195  OriginalContractSize(4)
    // 199  ContractMultiplier(4)
    // 203  ContractMultiplierUnit(1)
    // 204  FlowScheduleType(1)
    // 205  MinPriceIncrementAmount(8)
    // 213  UserDefinedInstrument(1)
    // 214  TradingReferenceDate(2)
    static constexpr uint16_t BLOCK_LENGTH = 216;

    uint8_t matchEventIndicator = 0;
    uint32_t totNumReports = 0;
    char securityUpdateAction = 'A';
    uint64_t lastUpdateTime = 0;
    uint8_t mdSecurityTradingStatus = 0;
    int16_t applID = 0;
    uint8_t marketSegmentID = 0;
    uint8_t underlyingProduct = 0;
    char securityExchange[4]{};
    char securityGroup[6]{};
    char asset[6]{};
    char symbol[20]{};
    int32_t securityID = 0;
    char securityType[6]{};
    char cfiCode[6]{};
    char maturityMonthYear[5]{};
    char currency[3]{};
    char settlCurrency[3]{};
    char matchAlgorithm = 'F';
    uint32_t minTradeVol = 1;
    uint32_t maxTradeVol = 10000;
    int64_t minPriceIncrement = 0;
    int64_t displayFactor = 0;
    uint8_t mainFraction = 0;
    uint8_t subFraction = 0;
    uint8_t priceDisplayFormat = 0;
    char unitOfMeasure[30]{};
    int64_t unitOfMeasureQty = 0;
    int64_t tradingReferencePrice = 0;
    uint8_t settlPriceType = 0;
    int32_t openInterestQty = 0;
    int32_t clearedVolume = 0;
    int64_t highLimitPrice = 0;
    int64_t lowLimitPrice = 0;
    int64_t maxPriceVariation = 0;
    int32_t decayQuantity = 0;
    uint16_t decayStartDate = 0;
    int32_t originalContractSize = 0;
    int32_t contractMultiplier = 0;
    uint8_t contractMultiplierUnit = 0;
    uint8_t flowScheduleType = 0;
    int64_t minPriceIncrementAmount = 0;
    char userDefinedInstrument = 'N';
    uint16_t tradingReferenceDate = 0;

    size_t encode(char* buffer, size_t offset) const {
        MessageHeader::encodeMDP3(buffer + offset, BLOCK_LENGTH, TEMPLATE_ID);
        char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(b + 0,   &matchEventIndicator, 1);
        std::memcpy(b + 1,   &totNumReports, 4);
        std::memcpy(b + 5,   &securityUpdateAction, 1);
        std::memcpy(b + 6,   &lastUpdateTime, 8);
        std::memcpy(b + 14,  &mdSecurityTradingStatus, 1);
        std::memcpy(b + 15,  &applID, 2);
        std::memcpy(b + 17,  &marketSegmentID, 1);
        std::memcpy(b + 18,  &underlyingProduct, 1);
        std::memcpy(b + 19,  securityExchange, 4);
        std::memcpy(b + 23,  securityGroup, 6);
        std::memcpy(b + 29,  asset, 6);
        std::memcpy(b + 35,  symbol, 20);
        std::memcpy(b + 55,  &securityID, 4);
        std::memcpy(b + 59,  securityType, 6);
        std::memcpy(b + 65,  cfiCode, 6);
        std::memcpy(b + 71,  maturityMonthYear, 5);
        std::memcpy(b + 76,  currency, 3);
        std::memcpy(b + 79,  settlCurrency, 3);
        std::memcpy(b + 82,  &matchAlgorithm, 1);
        std::memcpy(b + 83,  &minTradeVol, 4);
        std::memcpy(b + 87,  &maxTradeVol, 4);
        std::memcpy(b + 91,  &minPriceIncrement, 8);
        std::memcpy(b + 99,  &displayFactor, 8);
        std::memcpy(b + 107, &mainFraction, 1);
        std::memcpy(b + 108, &subFraction, 1);
        std::memcpy(b + 109, &priceDisplayFormat, 1);
        std::memcpy(b + 110, unitOfMeasure, 30);
        std::memcpy(b + 140, &unitOfMeasureQty, 8);
        std::memcpy(b + 148, &tradingReferencePrice, 8);
        std::memcpy(b + 156, &settlPriceType, 1);
        std::memcpy(b + 157, &openInterestQty, 4);
        std::memcpy(b + 161, &clearedVolume, 4);
        std::memcpy(b + 165, &highLimitPrice, 8);
        std::memcpy(b + 173, &lowLimitPrice, 8);
        std::memcpy(b + 181, &maxPriceVariation, 8);
        std::memcpy(b + 189, &decayQuantity, 4);
        std::memcpy(b + 193, &decayStartDate, 2);
        std::memcpy(b + 195, &originalContractSize, 4);
        std::memcpy(b + 199, &contractMultiplier, 4);
        std::memcpy(b + 203, &contractMultiplierUnit, 1);
        std::memcpy(b + 204, &flowScheduleType, 1);
        std::memcpy(b + 205, &minPriceIncrementAmount, 8);
        std::memcpy(b + 213, &userDefinedInstrument, 1);
        std::memcpy(b + 214, &tradingReferenceDate, 2);
        return MessageHeader::SIZE + BLOCK_LENGTH;
    }

    void decode(const char* buffer, size_t offset) {
        const char* b = buffer + offset + MessageHeader::SIZE;
        std::memcpy(&matchEventIndicator, b + 0, 1);
        std::memcpy(&totNumReports, b + 1, 4);
        std::memcpy(&securityUpdateAction, b + 5, 1);
        std::memcpy(&lastUpdateTime, b + 6, 8);
        std::memcpy(&mdSecurityTradingStatus, b + 14, 1);
        std::memcpy(&applID, b + 15, 2);
        std::memcpy(&marketSegmentID, b + 17, 1);
        std::memcpy(&underlyingProduct, b + 18, 1);
        std::memcpy(securityExchange, b + 19, 4);
        std::memcpy(securityGroup, b + 23, 6);
        std::memcpy(asset, b + 29, 6);
        std::memcpy(symbol, b + 35, 20);
        std::memcpy(&securityID, b + 55, 4);
        std::memcpy(securityType, b + 59, 6);
        std::memcpy(cfiCode, b + 65, 6);
        std::memcpy(maturityMonthYear, b + 71, 5);
        std::memcpy(currency, b + 76, 3);
        std::memcpy(settlCurrency, b + 79, 3);
        std::memcpy(&matchAlgorithm, b + 82, 1);
        std::memcpy(&minTradeVol, b + 83, 4);
        std::memcpy(&maxTradeVol, b + 87, 4);
        std::memcpy(&minPriceIncrement, b + 91, 8);
        std::memcpy(&displayFactor, b + 99, 8);
        std::memcpy(&mainFraction, b + 107, 1);
        std::memcpy(&subFraction, b + 108, 1);
        std::memcpy(&priceDisplayFormat, b + 109, 1);
        std::memcpy(unitOfMeasure, b + 110, 30);
        std::memcpy(&unitOfMeasureQty, b + 140, 8);
        std::memcpy(&tradingReferencePrice, b + 148, 8);
        std::memcpy(&settlPriceType, b + 156, 1);
        std::memcpy(&openInterestQty, b + 157, 4);
        std::memcpy(&clearedVolume, b + 161, 4);
        std::memcpy(&highLimitPrice, b + 165, 8);
        std::memcpy(&lowLimitPrice, b + 173, 8);
        std::memcpy(&maxPriceVariation, b + 181, 8);
        std::memcpy(&decayQuantity, b + 189, 4);
        std::memcpy(&decayStartDate, b + 193, 2);
        std::memcpy(&originalContractSize, b + 195, 4);
        std::memcpy(&contractMultiplier, b + 199, 4);
        std::memcpy(&contractMultiplierUnit, b + 203, 1);
        std::memcpy(&flowScheduleType, b + 204, 1);
        std::memcpy(&minPriceIncrementAmount, b + 205, 8);
        std::memcpy(&userDefinedInstrument, b + 213, 1);
        std::memcpy(&tradingReferenceDate, b + 214, 2);
    }

    size_t encodedLength() const { return MessageHeader::SIZE + BLOCK_LENGTH; }
};

} // namespace cme::sim::sbe
