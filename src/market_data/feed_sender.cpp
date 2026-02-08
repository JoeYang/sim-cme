#include "market_data/feed_sender.h"

namespace cme::sim::market_data {

FeedSender::FeedSender(boost::asio::io_context& io_ctx,
                       const std::string& multicast_addr,
                       uint16_t port,
                       const std::string& interface_addr)
    : sender_(std::make_unique<cme::sim::network::UdpMulticastSender>(
          io_ctx, multicast_addr, port, interface_addr)) {}

void FeedSender::send(const std::vector<uint8_t>& sbe_messages) {
    uint64_t ts = now();
    sender_->send(next_seq_num_, ts, sbe_messages);
    ++next_seq_num_;
}

void FeedSender::sendWithSeqNum(uint32_t seq_num,
                                 const std::vector<uint8_t>& sbe_messages) {
    uint64_t ts = now();
    sender_->send(seq_num, ts, sbe_messages);
}

uint64_t FeedSender::now() {
    auto tp = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            tp.time_since_epoch())
            .count());
}

} // namespace cme::sim::market_data
