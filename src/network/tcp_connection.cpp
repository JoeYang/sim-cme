#include "network/tcp_connection.h"

#include "common/endian_utils.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace cme::sim::network {

TcpConnection::TcpConnection(boost::asio::ip::tcp::socket socket)
    : socket_(std::move(socket))
    , strand_(boost::asio::make_strand(socket_.get_executor())) {
    boost::system::error_code ec;
    auto ep = socket_.remote_endpoint(ec);
    if (!ec) {
        remote_endpoint_str_ = ep.address().to_string() + ":" + std::to_string(ep.port());
    } else {
        remote_endpoint_str_ = "<unknown>";
    }
}

TcpConnection::~TcpConnection() {
    close();
}

TcpConnection::Ptr TcpConnection::create(boost::asio::ip::tcp::socket socket) {
    return Ptr(new TcpConnection(std::move(socket)));
}

void TcpConnection::start(MessageCallback on_message, DisconnectCallback on_disconnect) {
    on_message_ = std::move(on_message);
    on_disconnect_ = std::move(on_disconnect);
    spdlog::info("TCP connection from {}", remote_endpoint_str_);
    do_read_header();
}

void TcpConnection::send(const std::vector<std::uint8_t>& payload) {
    send(payload.data(), payload.size());
}

void TcpConnection::send(const std::uint8_t* data, std::size_t len) {
    // Build SOFH header + payload in a single buffer.
    auto msg = std::make_shared<std::vector<std::uint8_t>>(kSofhSize + len);

    // Message_Length includes the SOFH itself.
    auto msg_length = static_cast<std::uint32_t>(kSofhSize + len);
    auto encoding_type = static_cast<std::uint16_t>(kSofhEncodingSbe);

    cme::sim::endian::native_to_big_inplace(msg_length);
    cme::sim::endian::native_to_big_inplace(encoding_type);

    std::memcpy(msg->data(), &msg_length, 4);
    std::memcpy(msg->data() + 4, &encoding_type, 2);
    std::memcpy(msg->data() + kSofhSize, data, len);

    auto self = shared_from_this();
    boost::asio::post(strand_, [this, self, msg] {
        if (closed_) return;
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(*msg),
            boost::asio::bind_executor(strand_,
                [this, self, msg](boost::system::error_code ec, std::size_t /*bytes*/) {
                    if (ec) {
                        spdlog::warn("Write error to {}: {}", remote_endpoint_str_, ec.message());
                        close();
                    }
                }));
    });
}

void TcpConnection::close() {
    if (closed_) return;
    closed_ = true;

    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);

    spdlog::info("TCP connection to {} closed", remote_endpoint_str_);

    if (on_disconnect_) {
        auto cb = std::move(on_disconnect_);
        cb(shared_from_this());
    }
}

void TcpConnection::do_read_header() {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(header_buf_),
        boost::asio::bind_executor(strand_,
            [this, self](boost::system::error_code ec, std::size_t /*bytes*/) {
                if (ec) {
                    if (ec != boost::asio::error::eof &&
                        ec != boost::asio::error::operation_aborted) {
                        spdlog::warn("Read header error from {}: {}",
                                     remote_endpoint_str_, ec.message());
                    }
                    if (!closed_) close();
                    return;
                }

                // Parse SOFH: 4 bytes message_length (big-endian), 2 bytes encoding_type
                std::uint32_t msg_length = 0;
                std::uint16_t enc_type = 0;
                std::memcpy(&msg_length, header_buf_.data(), 4);
                std::memcpy(&enc_type, header_buf_.data() + 4, 2);

                cme::sim::endian::big_to_native_inplace(msg_length);
                cme::sim::endian::big_to_native_inplace(enc_type);

                if (enc_type != kSofhEncodingSbe) {
                    spdlog::warn("Unknown SOFH encoding 0x{:04X} from {}, closing",
                                 enc_type, remote_endpoint_str_);
                    close();
                    return;
                }

                if (msg_length <= kSofhSize) {
                    spdlog::warn("Invalid SOFH message length {} from {}, closing",
                                 msg_length, remote_endpoint_str_);
                    close();
                    return;
                }

                auto body_length = msg_length - static_cast<std::uint32_t>(kSofhSize);

                // Sanity check: reject absurdly large messages.
                constexpr std::uint32_t kMaxBody = 64 * 1024;
                if (body_length > kMaxBody) {
                    spdlog::warn("Message body too large ({} bytes) from {}, closing",
                                 body_length, remote_endpoint_str_);
                    close();
                    return;
                }

                do_read_body(body_length);
            }));
}

void TcpConnection::do_read_body(std::uint32_t body_length) {
    body_buf_.resize(body_length);
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(body_buf_),
        boost::asio::bind_executor(strand_,
            [this, self](boost::system::error_code ec, std::size_t /*bytes*/) {
                if (ec) {
                    if (ec != boost::asio::error::eof &&
                        ec != boost::asio::error::operation_aborted) {
                        spdlog::warn("Read body error from {}: {}",
                                     remote_endpoint_str_, ec.message());
                    }
                    if (!closed_) close();
                    return;
                }

                if (on_message_) {
                    on_message_(shared_from_this(), std::move(body_buf_));
                }

                // Continue reading the next message.
                do_read_header();
            }));
}

} // namespace cme::sim::network
