#pragma once

#include "../common/asio_compat.h"
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace cme::sim::network {

/// Pool of boost::asio::io_context instances, each running on its own thread.
/// Connections are distributed across contexts in round-robin fashion.
class IoContextPool {
public:
    /// Create a pool with @p pool_size io_context instances.
    /// If pool_size is 0, defaults to std::thread::hardware_concurrency().
    explicit IoContextPool(std::size_t pool_size = 0);

    ~IoContextPool();

    IoContextPool(const IoContextPool&) = delete;
    IoContextPool& operator=(const IoContextPool&) = delete;

    /// Start all threads. Each thread calls io_context::run().
    void start();

    /// Signal all io_contexts to stop and join all threads.
    void stop();

    /// Get the next io_context in round-robin order.
    boost::asio::io_context& get_io_context();

    std::size_t size() const { return io_contexts_.size(); }

private:
    using work_guard_t = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    std::vector<std::unique_ptr<boost::asio::io_context>> io_contexts_;
    std::vector<work_guard_t> work_guards_;
    std::vector<std::thread> threads_;
    std::size_t next_io_context_{0};
};

} // namespace cme::sim::network
