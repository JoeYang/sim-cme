#include "network/udp_multicast_sender.h"

#include "common/endian_utils.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace cme::sim::network {

UdpMulticastSender::UdpMulticastSender(boost::asio::io_context& io_ctx,
                                       const std::string& group,
                                       std::uint16_t port,
                                       const std::string& iface)
    : group_(group)
    , port_(port)
    , socket_(io_ctx, boost::asio::ip::udp::v4())
    , endpoint_(boost::asio::ip::make_address(group), port) {
    // Set outbound interface.
    socket_.set_option(
        boost::asio::ip::multicast::outbound_interface(
            boost::asio::ip::make_address_v4(iface)));

    // Set TTL to 1 (local subnet only) for safety.
    socket_.set_option(boost::asio::ip::multicast::hops(1));

    // Allow loopback so a listener on the same host can receive.
    socket_.set_option(boost::asio::ip::multicast::enable_loopback(true));

    spdlog::info("UdpMulticastSender ready on {}:{} (iface={})", group, port, iface);
}

UdpMulticastSender::~UdpMulticastSender() {
    close();
}

void UdpMulticastSender::send(std::uint32_t seq_num,
                               std::uint64_t sending_time,
                               const std::uint8_t* sbe_messages,
                               std::size_t sbe_len) {
    // Build packet: [MDP3 header (12 bytes)] [SBE messages]
    std::vector<std::uint8_t> packet(kMdp3PacketHeaderSize + sbe_len);

    // MDP 3.0 uses little-endian for the packet header.
    auto le_seq = cme::sim::endian::native_to_little(seq_num);
    auto le_time = cme::sim::endian::native_to_little(sending_time);

    std::memcpy(packet.data(), &le_seq, 4);
    std::memcpy(packet.data() + 4, &le_time, 8);

    if (sbe_len > 0) {
        std::memcpy(packet.data() + kMdp3PacketHeaderSize, sbe_messages, sbe_len);
    }

    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(packet), endpoint_, 0, ec);
    if (ec) {
        spdlog::warn("Multicast send error on {}:{}: {}", group_, port_, ec.message());
    }
}

void UdpMulticastSender::send(std::uint32_t seq_num,
                               std::uint64_t sending_time,
                               const std::vector<std::uint8_t>& sbe_messages) {
    send(seq_num, sending_time, sbe_messages.data(), sbe_messages.size());
}

void UdpMulticastSender::close() {
    boost::system::error_code ec;
    socket_.close(ec);
}

} // namespace cme::sim::network
