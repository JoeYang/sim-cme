#pragma once
#include <cstdint>
#include <cstring>

namespace cme::sim::sbe {

// SBE Message Header (8 bytes)
// Layout:
//   [2 bytes: blockLength (uint16_t LE)]
//   [2 bytes: templateId  (uint16_t LE)]
//   [2 bytes: schemaId    (uint16_t LE)]
//   [2 bytes: version     (uint16_t LE)]
struct MessageHeader {
    static constexpr size_t SIZE = 8;

    // iLink 3 schema constants
    static constexpr uint16_t ILINK3_SCHEMA_ID = 8;
    static constexpr uint16_t ILINK3_VERSION   = 8;

    // MDP 3.0 schema constants
    static constexpr uint16_t MDP3_SCHEMA_ID = 1;
    static constexpr uint16_t MDP3_VERSION   = 9;

    static void encode(char* buffer, uint16_t blockLength, uint16_t templateId,
                       uint16_t schemaId, uint16_t version) {
        std::memcpy(buffer,     &blockLength, 2);
        std::memcpy(buffer + 2, &templateId,  2);
        std::memcpy(buffer + 4, &schemaId,    2);
        std::memcpy(buffer + 6, &version,     2);
    }

    static void encodeILink3(char* buffer, uint16_t blockLength, uint16_t templateId) {
        encode(buffer, blockLength, templateId, ILINK3_SCHEMA_ID, ILINK3_VERSION);
    }

    static void encodeMDP3(char* buffer, uint16_t blockLength, uint16_t templateId) {
        encode(buffer, blockLength, templateId, MDP3_SCHEMA_ID, MDP3_VERSION);
    }

    static uint16_t decodeBlockLength(const char* buffer) {
        uint16_t val;
        std::memcpy(&val, buffer, 2);
        return val;
    }

    static uint16_t decodeTemplateId(const char* buffer) {
        uint16_t val;
        std::memcpy(&val, buffer + 2, 2);
        return val;
    }

    static uint16_t decodeSchemaId(const char* buffer) {
        uint16_t val;
        std::memcpy(&val, buffer + 4, 2);
        return val;
    }

    static uint16_t decodeVersion(const char* buffer) {
        uint16_t val;
        std::memcpy(&val, buffer + 6, 2);
        return val;
    }
};

} // namespace cme::sim::sbe
