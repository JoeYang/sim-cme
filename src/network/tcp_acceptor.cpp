#include "network/tcp_acceptor.h"
#include "network/io_context_pool.h"

#include <spdlog/spdlog.h>

namespace cme::sim::network {

TcpAcceptor::TcpAcceptor(boost::asio::io_context& acceptor_ctx,
                         IoContextPool& pool,
                         const std::string& address,
                         std::uint16_t port)
    : pool_(pool)
    , acceptor_(acceptor_ctx) {
    namespace ip = boost::asio::ip;

    ip::tcp::endpoint endpoint(ip::make_address(address), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    spdlog::info("TcpAcceptor listening on {}:{}", address, port);
}

TcpAcceptor::~TcpAcceptor() {
    stop();
}

void TcpAcceptor::start(ConnectionCallback on_connection) {
    on_connection_ = std::move(on_connection);
    do_accept();
}

void TcpAcceptor::stop() {
    stopped_ = true;

    boost::system::error_code ec;
    acceptor_.close(ec);

    std::lock_guard lock(connections_mu_);
    for (auto& conn : connections_) {
        conn->close();
    }
    connections_.clear();
}

std::size_t TcpAcceptor::connection_count() const {
    std::lock_guard lock(connections_mu_);
    return connections_.size();
}

void TcpAcceptor::do_accept() {
    if (stopped_) return;

    auto& io_ctx = pool_.get_io_context();
    acceptor_.async_accept(
        io_ctx,
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (ec) {
                if (ec != boost::asio::error::operation_aborted) {
                    spdlog::error("Accept error: {}", ec.message());
                }
                return;
            }

            // Disable Nagle's algorithm for low-latency messaging.
            socket.set_option(boost::asio::ip::tcp::no_delay(true));

            auto conn = TcpConnection::create(std::move(socket));

            {
                std::lock_guard lock(connections_mu_);
                connections_.insert(conn);
            }

            if (on_connection_) {
                on_connection_(conn);
            }

            // Continue accepting.
            do_accept();
        });
}

void TcpAcceptor::remove_connection(const TcpConnection::Ptr& conn) {
    std::lock_guard lock(connections_mu_);
    connections_.erase(conn);
}

} // namespace cme::sim::network
