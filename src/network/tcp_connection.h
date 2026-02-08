#pragma once

#include "../common/asio_compat.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cme::sim::network {

/// Simple FIXP Header (SOFH) layout:
///   Bytes 0-3 : Message_Length (uint32_t, big-endian, includes the 6-byte SOFH itself)
///   Bytes 4-5 : Encoding_Type (uint16_t, big-endian, 0xCAFE = SBE v1.0)
static constexpr std::size_t kSofhSize = 6;
static constexpr std::uint16_t kSofhEncodingSbe = 0xCAFE;

/// Represents a single TCP connection to a client.
/// Handles SOFH-framed message reading and writing.
class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    using Ptr = std::shared_ptr<TcpConnection>;
    using MessageCallback = std::function<void(Ptr, std::vector<std::uint8_t>)>;
    using DisconnectCallback = std::function<void(Ptr)>;

    /// Create a connection owning the given socket.
    static Ptr create(boost::asio::ip::tcp::socket socket);

    ~TcpConnection();

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    /// Begin asynchronous read loop. Calls on_message for each complete SOFH
    /// frame, and on_disconnect when the connection closes or errors.
    void start(MessageCallback on_message, DisconnectCallback on_disconnect);

    /// Write an SOFH-framed message. The payload should be raw SBE bytes
    /// (without the SOFH header); the header is prepended automatically.
    void send(const std::vector<std::uint8_t>& payload);
    void send(const std::uint8_t* data, std::size_t len);

    /// Gracefully close the connection.
    void close();

    /// Connection identifier for logging.
    std::string remote_endpoint_str() const { return remote_endpoint_str_; }

    boost::asio::ip::tcp::socket& socket() { return socket_; }

private:
    explicit TcpConnection(boost::asio::ip::tcp::socket socket);

    void do_read_header();
    void do_read_body(std::uint32_t body_length);

    boost::asio::ip::tcp::socket socket_;
    boost::asio::strand<boost::asio::ip::tcp::socket::executor_type> strand_;

    std::string remote_endpoint_str_;
    std::array<std::uint8_t, kSofhSize> header_buf_{};
    std::vector<std::uint8_t> body_buf_;

    MessageCallback on_message_;
    DisconnectCallback on_disconnect_;
    bool closed_{false};
};

} // namespace cme::sim::network
