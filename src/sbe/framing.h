#pragma once
#include <cstdint>
#include <cstring>
#include "../common/endian_utils.h"

namespace cme::sim::sbe {

// Simple Open Framing Header (SOFH) for iLink 3 TCP
// Layout (per CME spec — big-endian):
//   [4 bytes: message_length (uint32_t BE, includes SOFH itself)]
//   [2 bytes: encoding_type (uint16_t BE, 0xCAFE for SBE)]
struct SOFH {
    static constexpr size_t SIZE = 6;
    static constexpr uint16_t SBE_ENCODING_TYPE = 0xCAFE;

    static void encode(char* buffer, uint32_t message_length) {
        endian::native_to_big_inplace(message_length);
        std::memcpy(buffer, &message_length, 4);
        uint16_t encoding = SBE_ENCODING_TYPE;
        endian::native_to_big_inplace(encoding);
        std::memcpy(buffer + 4, &encoding, 2);
    }

    static uint32_t decodeMessageLength(const char* buffer) {
        uint32_t length;
        std::memcpy(&length, buffer, 4);
        endian::big_to_native_inplace(length);
        return length;
    }

    static uint16_t decodeEncodingType(const char* buffer) {
        uint16_t encoding;
        std::memcpy(&encoding, buffer + 4, 2);
        endian::big_to_native_inplace(encoding);
        return encoding;
    }

    static bool isValidSBE(const char* buffer) {
        return decodeEncodingType(buffer) == SBE_ENCODING_TYPE;
    }

    // Returns the total framed message length including SOFH
    static uint32_t framedLength(uint32_t payload_length) {
        return static_cast<uint32_t>(SIZE) + payload_length;
    }
};

} // namespace cme::sim::sbe
