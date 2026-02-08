#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace cme::sim::config {

struct NetworkConfig {
    std::string tcp_listen_address = "0.0.0.0";
    uint16_t tcp_listen_port = 9563;
    std::string multicast_base_address = "239.1.1";
    uint16_t multicast_base_port = 14310;
    int io_threads = 2;
};

struct FeedConfig {
    std::string address_a;
    uint16_t port_a = 0;
    std::string address_b;
    uint16_t port_b = 0;
};

struct ChannelConfig {
    int channel_id = 0;
    std::string name;
    FeedConfig incremental_feed;
    FeedConfig snapshot_feed;
    FeedConfig instrument_def_feed;
    std::vector<std::string> symbols;
};

struct InstrumentConfig {
    std::string symbol;
    int32_t security_id = 0;
    int channel_id = 0;
    double tick_size = 0.0;
    double contract_multiplier = 0.0;
    double min_price_increment_amount = 0.0; // tick_size * multiplier
    int32_t min_trade_vol = 1;
    int32_t max_trade_vol = 10000;
    std::string maturity_month_year; // e.g. "202503"
    double display_factor = 0.01;
};

struct EngineConfig {
    std::string mode = "full_matching"; // or "synthetic"
    std::string pcap_path; // for synthetic mode
    double synthetic_fill_probability = 1.0;
    uint64_t synthetic_fill_latency_ns = 1000;
};

struct RiskConfig {
    int32_t max_order_qty = 10000;
    double max_price_deviation_pct = 10.0; // from last trade
    int32_t max_orders_per_second = 1000;
    int64_t max_position_per_session = 50000;
};

struct SessionConfig {
    bool hmac_enabled = false;
    std::string hmac_key = "test_key";
    uint32_t keep_alive_interval_ms = 30000;
    int max_sessions = 100;
    int retransmit_buffer_size = 10000;
};

struct ExchangeConfig {
    NetworkConfig network;
    std::vector<ChannelConfig> channels;
    std::vector<InstrumentConfig> instruments;
    EngineConfig engine;
    RiskConfig risk;
    SessionConfig session;
    std::string log_level = "info";
};

} // namespace cme::sim::config
