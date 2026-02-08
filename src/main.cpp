// CME Exchange Simulator - Application Orchestrator
// Wires together: Config -> Instruments -> Engine -> Gateway -> FIXP Sessions -> TCP -> Market Data

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "config/config_loader.h"
#include "config/exchange_config.h"
#include "common/asio_compat.h"
#include "common/logger.h"
#include "common/types.h"
#include "network/io_context_pool.h"
#include "network/tcp_acceptor.h"
#include "fixp/session_manager.h"
#include "gateway/order_entry_gateway.h"
#include "engine/full_matching_engine.h"
#include "engine/engine_event.h"
#include "market_data/market_data_publisher.h"
#include "instruments/instrument_manager.h"

using namespace cme::sim;
using namespace cme::sim::config;
using namespace cme::sim::fixp;
using namespace cme::sim::gateway;
using namespace cme::sim::market_data;
using namespace cme::sim::network;

// ---------------------------------------------------------------------------
// Global shutdown flag, set by signal handler
// ---------------------------------------------------------------------------
static std::atomic<bool> g_running{true};

static void signalHandler(int sig) {
    (void)sig;
    g_running.store(false, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Command-line argument parsing
// ---------------------------------------------------------------------------
struct AppArgs {
    std::string config_path = "config/exchange_config.yaml";
    std::string log_level;   // empty => use config value
    std::string mode;        // empty => use config value
};

static AppArgs parseArgs(int argc, char* argv[]) {
    AppArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--config") && i + 1 < argc) {
            args.config_path = argv[++i];
        } else if ((a == "--log-level") && i + 1 < argc) {
            args.log_level = argv[++i];
        } else if ((a == "--mode") && i + 1 < argc) {
            args.mode = argv[++i];
        } else if (a == "--help" || a == "-h") {
            std::cout << "Usage: sim_cme_exchange [OPTIONS]\n"
                      << "  --config PATH       Config file (default: config/exchange_config.yaml)\n"
                      << "  --log-level LEVEL   debug|info|warn|error (overrides config)\n"
                      << "  --mode MODE         full_matching|synthetic (overrides config)\n"
                      << "  --help              Show this message\n";
            std::exit(EXIT_SUCCESS);
        }
    }
    return args;
}

// ---------------------------------------------------------------------------
// Logging initialisation
// ---------------------------------------------------------------------------
static void initLogging(const std::string& level_str) {
    spdlog::level::level_enum lvl = spdlog::level::info;
    if (level_str == "debug")       lvl = spdlog::level::debug;
    else if (level_str == "info")   lvl = spdlog::level::info;
    else if (level_str == "warn")   lvl = spdlog::level::warn;
    else if (level_str == "error")  lvl = spdlog::level::err;
    else if (level_str == "trace")  lvl = spdlog::level::trace;

    spdlog::set_level(lvl);
}

// ---------------------------------------------------------------------------
// Startup banner
// ---------------------------------------------------------------------------
static void printBanner(const ExchangeConfig& cfg,
                        const InstrumentManager& instruments) {
    std::string mode_display = (cfg.engine.mode == "full_matching")
        ? "Full Matching Engine"
        : "Synthetic (pcap replay)";

    std::cout << "\n";
    std::cout << "======================================================\n";
    std::cout << "       CME Exchange Simulator v1.0\n";
    std::cout << "  Mode: " << mode_display << "\n";
    std::cout << "  TCP Order Entry: " << cfg.network.tcp_listen_address
              << ":" << cfg.network.tcp_listen_port << "\n";

    // List channels
    std::cout << "  Channels:";
    for (size_t i = 0; i < cfg.channels.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << " " << cfg.channels[i].channel_id
                  << " (" << cfg.channels[i].name << ")";
    }
    std::cout << "\n";

    std::cout << "  Instruments: " << instruments.getAllInstruments().size()
              << " loaded\n";
    std::cout << "  Log Level: " << cfg.log_level << "\n";
    std::cout << "======================================================\n";
    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// Book snapshot provider for market data snapshot cycler
// ---------------------------------------------------------------------------
static SnapshotCycler::BookSnapshotProvider makeBookSnapshotProvider(
        FullMatchingEngine& engine) {
    return [&engine](SecurityId sec_id,
                     std::vector<std::pair<Price, Quantity>>& bids,
                     std::vector<std::pair<Price, Quantity>>& asks,
                     std::vector<int>& bid_counts,
                     std::vector<int>& ask_counts) {
        const OrderBook* book = engine.getOrderBook(sec_id);
        if (!book) return;

        bids.clear();
        asks.clear();
        bid_counts.clear();
        ask_counts.clear();

        for (auto& [price, level] : book->bidLevels()) {
            bids.emplace_back(price, level.total_quantity);
            bid_counts.push_back(level.order_count);
        }
        for (auto& [price, level] : book->askLevels()) {
            asks.emplace_back(price, level.total_quantity);
            ask_counts.push_back(level.order_count);
        }
    };
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // 1. Parse command-line arguments
    AppArgs args = parseArgs(argc, argv);

    // 2. Load configuration
    ExchangeConfig cfg;
    try {
        cfg = loadConfig(args.config_path);
        validateConfig(cfg);
    } catch (const std::exception& ex) {
        std::cerr << "Configuration error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    // Apply CLI overrides
    if (!args.log_level.empty()) {
        cfg.log_level = args.log_level;
    }
    if (!args.mode.empty()) {
        cfg.engine.mode = args.mode;
    }

    // 3. Initialize logging
    initLogging(cfg.log_level);
    auto logger = getLogger("MAIN");
    logger->info("Loading configuration from {}", args.config_path);

    // 4. Create instrument manager and load instruments
    InstrumentManager instrument_mgr;
    instrument_mgr.loadFromConfig(cfg.instruments, cfg.channels);
    logger->info("Loaded {} instruments across {} channels",
                 instrument_mgr.getAllInstruments().size(), cfg.channels.size());

    // Set all instruments to PreOpen initially
    for (auto& inst : instrument_mgr.getAllInstruments()) {
        instrument_mgr.setTradingStatus(inst.security_id,
                                        SecurityTradingStatus::PreOpen);
    }

    // 5. Create matching engine based on mode
    std::unique_ptr<IMatchingEngine> engine;
    FullMatchingEngine* full_engine_ptr = nullptr;

    if (cfg.engine.mode == "full_matching") {
        auto full_engine = std::make_unique<FullMatchingEngine>();
        for (auto& inst : instrument_mgr.getAllInstruments()) {
            full_engine->addInstrument(inst.security_id);
        }
        full_engine_ptr = full_engine.get();
        engine = std::move(full_engine);
        logger->info("Created Full Matching Engine with {} order books",
                     instrument_mgr.getAllInstruments().size());
    } else if (cfg.engine.mode == "synthetic") {
        // Synthetic engine is optional -- if not linked, fall back to full matching
        logger->warn("Synthetic engine mode requested but not yet available; "
                     "falling back to full_matching mode");
        cfg.engine.mode = "full_matching";
        auto full_engine = std::make_unique<FullMatchingEngine>();
        for (auto& inst : instrument_mgr.getAllInstruments()) {
            full_engine->addInstrument(inst.security_id);
        }
        full_engine_ptr = full_engine.get();
        engine = std::move(full_engine);
    } else {
        logger->error("Unknown engine mode: {}", cfg.engine.mode);
        return EXIT_FAILURE;
    }

    // 6. Create FIXP session manager
    SessionManager session_mgr(cfg.session.max_sessions);
    logger->info("Session manager created (max sessions: {})",
                 cfg.session.max_sessions);

    // 7. Create order entry gateway
    OrderEntryGateway gateway(instrument_mgr, cfg.risk);
    logger->info("Order entry gateway created");

    // 8. Create network layer
    IoContextPool io_pool(cfg.network.io_threads);

    // We need a separate io_context for the acceptor itself
    boost::asio::io_context acceptor_ctx;

    TcpAcceptor acceptor(acceptor_ctx, io_pool,
                                  cfg.network.tcp_listen_address,
                                  cfg.network.tcp_listen_port);

    // 9. Create market data publisher
    boost::asio::io_context md_io_ctx;
    MarketDataPublisher md_publisher(cfg.channels, instrument_mgr, md_io_ctx);

    // Set up book snapshot provider if we have a full matching engine
    if (full_engine_ptr) {
        md_publisher.setBookSnapshotProvider(
            makeBookSnapshotProvider(*full_engine_ptr));
    }

    // 10. Wire TCP connections to FIXP sessions
    acceptor.start([&](TcpConnection::Ptr conn) {
        logger->info("New TCP connection from {}", conn->remote_endpoint_str());

        // SendCallback: session -> TCP
        SendCallback send_cb = [conn](const char* data, size_t len) {
            conn->send(reinterpret_cast<const uint8_t*>(data), len);
        };

        // AppMessageCallback: session -> gateway
        AppMessageCallback app_cb = [&gateway](uint64_t uuid, uint16_t templateId,
                                               const char* data, size_t len) {
            gateway.onApplicationMessage(uuid, templateId, data, len);
        };

        auto session = session_mgr.createSession(send_cb, app_cb);
        if (!session) {
            logger->warn("Session limit reached, rejecting connection from {}",
                         conn->remote_endpoint_str());
            conn->close();
            return;
        }

        // Configure HMAC if enabled
        if (cfg.session.hmac_enabled) {
            session->setHmacEnabled(true);
            session->setHmacKey(cfg.session.hmac_key);
        }

        uint64_t session_uuid = session->uuid();
        logger->info("Created FIXP session UUID={}", session_uuid);

        // TCP message callback -> session
        auto weak_session = std::weak_ptr<Session>(session);
        conn->start(
            // on_message: TCP -> FIXP session
            [weak_session, logger](TcpConnection::Ptr /*conn*/,
                                   std::vector<uint8_t> msg) {
                auto sess = weak_session.lock();
                if (sess) {
                    sess->onMessage(reinterpret_cast<const char*>(msg.data()),
                                    msg.size());
                }
            },
            // on_disconnect: clean up session
            [&session_mgr, session_uuid, logger](TcpConnection::Ptr conn) {
                logger->info("TCP disconnect from {} (session UUID={})",
                             conn->remote_endpoint_str(), session_uuid);
                auto sess = session_mgr.findSession(session_uuid);
                if (sess && sess->state() != SessionState::Terminated) {
                    sess->terminate();
                }
                session_mgr.removeSession(session_uuid);
            }
        );
    });

    logger->info("TCP acceptor started on {}:{}",
                 cfg.network.tcp_listen_address, cfg.network.tcp_listen_port);

    // 11. Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 12. Print startup banner
    printBanner(cfg, instrument_mgr);

    // 13. Start IO context pool (network threads)
    io_pool.start();
    logger->info("IO context pool started ({} threads)", cfg.network.io_threads);

    // Start acceptor io_context in its own thread
    std::thread acceptor_thread([&acceptor_ctx]() {
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
            guard = boost::asio::make_work_guard(acceptor_ctx);
        acceptor_ctx.run();
    });

    // 14. Start market data publisher background threads
    md_publisher.start();
    logger->info("Market data publisher started");

    // Run md_io_ctx in a background thread
    std::thread md_io_thread([&md_io_ctx]() {
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
            guard = boost::asio::make_work_guard(md_io_ctx);
        md_io_ctx.run();
    });

    // 15. Open all instruments for trading
    for (auto& inst : instrument_mgr.getAllInstruments()) {
        instrument_mgr.setTradingStatus(inst.security_id,
                                        SecurityTradingStatus::Open);
    }
    logger->info("All instruments opened for trading");

    // 16. Engine thread (single-threaded hot path)
    std::thread engine_thread([&]() {
        logger->info("Engine thread started");

        while (g_running.load(std::memory_order_relaxed)) {
            // Drain commands from gateway, run through engine
            std::vector<EngineEvent> md_events;
            auto responses = gateway.processCommands(*engine, &md_events);

            // Route responses back to FIXP sessions
            for (auto& resp : responses) {
                auto session = session_mgr.findSession(resp.session_uuid);
                if (session) {
                    session->sendApplicationMessage(resp.sbe_message.data(),
                                                    resp.sbe_message.size());
                }
            }

            // Publish market data for all engine events (BookUpdate, OrderFilled, etc.)
            if (!md_events.empty()) {
                md_publisher.publishEvents(md_events);
            }

            // Brief sleep if no work to avoid busy spin
            if (responses.empty()) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }

        logger->info("Engine thread stopping");
    });

    // 17. Session keepalive timer thread
    std::thread timer_thread([&]() {
        while (g_running.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            session_mgr.onTimerTick();
        }
    });

    // ---------------------------------------------------------------------------
    // Main thread: wait for shutdown signal
    // ---------------------------------------------------------------------------
    logger->info("Exchange simulator running. Press Ctrl+C to stop.");

    while (g_running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    logger->info("Shutdown signal received");

    // 18. Graceful shutdown sequence
    // a. Stop accepting new connections
    acceptor.stop();
    logger->info("Stopped accepting new connections");

    // b. Close all instruments
    for (auto& inst : instrument_mgr.getAllInstruments()) {
        instrument_mgr.setTradingStatus(inst.security_id,
                                        SecurityTradingStatus::Close);
    }
    logger->info("All instruments closed");

    // c. Stop engine thread
    // g_running is already false, engine_thread will exit its loop
    if (engine_thread.joinable()) {
        engine_thread.join();
    }
    logger->info("Engine thread stopped");

    // d. Stop timer thread
    if (timer_thread.joinable()) {
        timer_thread.join();
    }

    // e. Stop market data publisher
    md_publisher.stop();
    md_io_ctx.stop();
    if (md_io_thread.joinable()) {
        md_io_thread.join();
    }
    logger->info("Market data publisher stopped");

    // f. Stop IO context pool and acceptor
    io_pool.stop();
    acceptor_ctx.stop();
    if (acceptor_thread.joinable()) {
        acceptor_thread.join();
    }
    logger->info("Network threads stopped");

    std::cout << "\n======================================================\n"
              << "  CME Exchange Simulator shutdown complete.\n"
              << "  Sessions served: "
              << acceptor.connection_count() << "\n"
              << "======================================================\n"
              << std::endl;

    spdlog::shutdown();
    return EXIT_SUCCESS;
}
