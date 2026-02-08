#pragma once

#include "../network/udp_multicast_sender.h"
#include "../common/asio_compat.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cme::sim::market_data {

/// Wrapper around UdpMulticastSender that manages MDP 3.0 packet framing
/// and per-feed sequence numbers. Each FeedSender represents one UDP feed
/// (e.g. incremental feed A, snapshot feed B, etc.).
class FeedSender {
public:
    FeedSender(boost::asio::io_context& io_ctx,
               const std::string& multicast_addr,
               uint16_t port,
               const std::string& interface_addr = "0.0.0.0");

    /// Send SBE message payload with auto-incrementing sequence number.
    /// Prepends MDP3 packet header: [4B MsgSeqNum][8B SendingTime].
    void send(const std::vector<uint8_t>& sbe_messages);

    /// Send with an explicit sequence number (does not advance internal counter).
    void sendWithSeqNum(uint32_t seq_num, const std::vector<uint8_t>& sbe_messages);

    uint32_t nextSeqNum() const { return next_seq_num_; }

    /// Current timestamp in nanoseconds since epoch.
    static uint64_t now();

private:
    std::unique_ptr<cme::sim::network::UdpMulticastSender> sender_;
    uint32_t next_seq_num_ = 1;
};

} // namespace cme::sim::market_data
