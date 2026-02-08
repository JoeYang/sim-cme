#pragma once
#include <cstdint>
#include <cstring>

namespace cme::sim::sbe {

// MDP 3.0 UDP Packet Header
// Layout:
//   [4 bytes: MsgSeqNum (uint32_t LE)]
//   [8 bytes: SendingTime (uint64_t LE, nanoseconds since epoch)]
struct PacketHeader {
    static constexpr size_t SIZE = 12;

    static void encode(char* buffer, uint32_t msgSeqNum, uint64_t sendingTime) {
        std::memcpy(buffer, &msgSeqNum, 4);
        std::memcpy(buffer + 4, &sendingTime, 8);
    }

    static uint32_t decodeMsgSeqNum(const char* buffer) {
        uint32_t val;
        std::memcpy(&val, buffer, 4);
        return val;
    }

    static uint64_t decodeSendingTime(const char* buffer) {
        uint64_t val;
        std::memcpy(&val, buffer + 4, 8);
        return val;
    }
};

} // namespace cme::sim::sbe
