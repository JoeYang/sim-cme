#include "network/io_context_pool.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

namespace cme::sim::network {

IoContextPool::IoContextPool(std::size_t pool_size) {
    if (pool_size == 0) {
        pool_size = std::thread::hardware_concurrency();
        if (pool_size == 0) {
            pool_size = 1;
        }
    }

    io_contexts_.reserve(pool_size);
    work_guards_.reserve(pool_size);

    for (std::size_t i = 0; i < pool_size; ++i) {
        auto ctx = std::make_unique<boost::asio::io_context>();
        work_guards_.emplace_back(boost::asio::make_work_guard(*ctx));
        io_contexts_.push_back(std::move(ctx));
    }

    spdlog::info("IoContextPool created with {} io_context(s)", pool_size);
}

IoContextPool::~IoContextPool() {
    stop();
}

void IoContextPool::start() {
    if (!threads_.empty()) {
        return; // already running
    }

    threads_.reserve(io_contexts_.size());
    for (std::size_t i = 0; i < io_contexts_.size(); ++i) {
        threads_.emplace_back([this, i] {
            spdlog::debug("IoContext thread {} started", i);
            io_contexts_[i]->run();
            spdlog::debug("IoContext thread {} stopped", i);
        });
    }
}

void IoContextPool::stop() {
    // Release work guards so io_context::run() can return when idle.
    for (auto& guard : work_guards_) {
        guard.reset();
    }

    // Stop all io_contexts.
    for (auto& ctx : io_contexts_) {
        ctx->stop();
    }

    // Join all threads.
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
}

boost::asio::io_context& IoContextPool::get_io_context() {
    auto& ctx = *io_contexts_[next_io_context_];
    next_io_context_ = (next_io_context_ + 1) % io_contexts_.size();
    return ctx;
}

} // namespace cme::sim::network
