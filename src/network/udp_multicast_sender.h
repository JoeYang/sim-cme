#pragma once

#include "../common/asio_compat.h"
#include <cstdint>
#include <string>
#include <vector>

namespace cme::sim::network {

/// MDP 3.0 binary packet header (12 bytes):
///   Bytes 0-3 : MsgSeqNum   (uint32_t, little-endian, per CME MDP spec)
///   Bytes 4-11: SendingTime  (uint64_t, little-endian, nanoseconds since epoch)
static constexpr std::size_t kMdp3PacketHeaderSize = 12;

/// Sends UDP multicast packets on a configurable group:port.
/// Supports Feed A + Feed B redundancy (two separate senders).
class UdpMulticastSender {
public:
    /// @param io_ctx    io_context for async operations
    /// @param group     multicast group address (e.g. "239.1.1.1")
    /// @param port      destination port
    /// @param iface     outbound interface address ("0.0.0.0" = default)
    UdpMulticastSender(boost::asio::io_context& io_ctx,
                       const std::string& group,
                       std::uint16_t port,
                       const std::string& iface = "0.0.0.0");

    ~UdpMulticastSender();

    UdpMulticastSender(const UdpMulticastSender&) = delete;
    UdpMulticastSender& operator=(const UdpMulticastSender&) = delete;

    /// Send an MDP3 packet. Prepends the 12-byte MDP 3.0 packet header
    /// (MsgSeqNum + SendingTime) to the payload of SBE messages.
    /// @param seq_num       Packet sequence number
    /// @param sending_time  Nanoseconds since epoch (UTC)
    /// @param sbe_messages  One or more concatenated SBE messages (body only)
    void send(std::uint32_t seq_num,
              std::uint64_t sending_time,
              const std::uint8_t* sbe_messages,
              std::size_t sbe_len);

    void send(std::uint32_t seq_num,
              std::uint64_t sending_time,
              const std::vector<std::uint8_t>& sbe_messages);

    /// Close the socket.
    void close();

    const std::string& group() const { return group_; }
    std::uint16_t port() const { return port_; }

private:
    std::string group_;
    std::uint16_t port_;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint endpoint_;
};

} // namespace cme::sim::network
