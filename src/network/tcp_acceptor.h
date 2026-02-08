#pragma once

#include "tcp_connection.h"

#include "../common/asio_compat.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

namespace cme::sim::network {

class IoContextPool;

/// TCP acceptor that listens on a configurable port, accepts connections,
/// and creates TcpConnection instances distributed across an IoContextPool.
class TcpAcceptor {
public:
    using ConnectionCallback = std::function<void(TcpConnection::Ptr)>;

    /// @param acceptor_ctx  io_context for the acceptor itself
    /// @param pool          io_context pool for accepted connections
    /// @param address       listen address (e.g. "0.0.0.0")
    /// @param port          listen port
    TcpAcceptor(boost::asio::io_context& acceptor_ctx,
                IoContextPool& pool,
                const std::string& address,
                std::uint16_t port);

    ~TcpAcceptor();

    TcpAcceptor(const TcpAcceptor&) = delete;
    TcpAcceptor& operator=(const TcpAcceptor&) = delete;

    /// Start accepting connections.
    /// @param on_connection  called for each new connection; the callback
    ///        should call conn->start() to begin the read loop.
    void start(ConnectionCallback on_connection);

    /// Stop accepting and close all tracked connections.
    void stop();

    std::size_t connection_count() const;

private:
    void do_accept();
    void remove_connection(const TcpConnection::Ptr& conn);

    IoContextPool& pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
    ConnectionCallback on_connection_;

    mutable std::mutex connections_mu_;
    std::unordered_set<TcpConnection::Ptr> connections_;
    bool stopped_{false};
};

} // namespace cme::sim::network
