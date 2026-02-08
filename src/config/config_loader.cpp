#include "config_loader.h"
#include "channel_config.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <set>

namespace cme::sim::config {

namespace {

FeedConfig parseFeed(const YAML::Node& node) {
    FeedConfig feed;
    if (!node || !node.IsMap()) return feed;
    if (node["address_a"]) feed.address_a = node["address_a"].as<std::string>();
    if (node["port_a"])    feed.port_a = node["port_a"].as<uint16_t>();
    if (node["address_b"]) feed.address_b = node["address_b"].as<std::string>();
    if (node["port_b"])    feed.port_b = node["port_b"].as<uint16_t>();
    return feed;
}

ChannelConfig parseChannel(const YAML::Node& node) {
    ChannelConfig ch;
    if (node["channel_id"]) ch.channel_id = node["channel_id"].as<int>();
    if (node["name"])       ch.name = node["name"].as<std::string>();
    if (node["incremental_feed"]) ch.incremental_feed = parseFeed(node["incremental_feed"]);
    if (node["snapshot_feed"])    ch.snapshot_feed = parseFeed(node["snapshot_feed"]);
    if (node["instrument_def_feed"]) ch.instrument_def_feed = parseFeed(node["instrument_def_feed"]);
    if (node["symbols"] && node["symbols"].IsSequence()) {
        for (const auto& s : node["symbols"]) {
            ch.symbols.push_back(s.as<std::string>());
        }
    }
    return ch;
}

InstrumentConfig parseInstrument(const YAML::Node& node) {
    InstrumentConfig inst;
    if (node["symbol"])                   inst.symbol = node["symbol"].as<std::string>();
    if (node["security_id"])              inst.security_id = node["security_id"].as<int32_t>();
    if (node["channel_id"])               inst.channel_id = node["channel_id"].as<int>();
    if (node["tick_size"])                inst.tick_size = node["tick_size"].as<double>();
    if (node["contract_multiplier"])      inst.contract_multiplier = node["contract_multiplier"].as<double>();
    if (node["min_price_increment_amount"]) inst.min_price_increment_amount = node["min_price_increment_amount"].as<double>();
    if (node["min_trade_vol"])            inst.min_trade_vol = node["min_trade_vol"].as<int32_t>();
    if (node["max_trade_vol"])            inst.max_trade_vol = node["max_trade_vol"].as<int32_t>();
    if (node["maturity_month_year"])      inst.maturity_month_year = node["maturity_month_year"].as<std::string>();
    if (node["display_factor"])           inst.display_factor = node["display_factor"].as<double>();
    return inst;
}

NetworkConfig parseNetwork(const YAML::Node& node) {
    NetworkConfig net;
    if (!node || !node.IsMap()) return net;
    if (node["tcp_listen_address"])    net.tcp_listen_address = node["tcp_listen_address"].as<std::string>();
    if (node["tcp_listen_port"])       net.tcp_listen_port = node["tcp_listen_port"].as<uint16_t>();
    if (node["multicast_base_address"]) net.multicast_base_address = node["multicast_base_address"].as<std::string>();
    if (node["multicast_base_port"])   net.multicast_base_port = node["multicast_base_port"].as<uint16_t>();
    if (node["io_threads"])            net.io_threads = node["io_threads"].as<int>();
    return net;
}

EngineConfig parseEngine(const YAML::Node& node) {
    EngineConfig eng;
    if (!node || !node.IsMap()) return eng;
    if (node["mode"])                      eng.mode = node["mode"].as<std::string>();
    if (node["pcap_path"])                 eng.pcap_path = node["pcap_path"].as<std::string>();
    if (node["synthetic_fill_probability"]) eng.synthetic_fill_probability = node["synthetic_fill_probability"].as<double>();
    if (node["synthetic_fill_latency_ns"])  eng.synthetic_fill_latency_ns = node["synthetic_fill_latency_ns"].as<uint64_t>();
    return eng;
}

RiskConfig parseRisk(const YAML::Node& node) {
    RiskConfig risk;
    if (!node || !node.IsMap()) return risk;
    if (node["max_order_qty"])           risk.max_order_qty = node["max_order_qty"].as<int32_t>();
    if (node["max_price_deviation_pct"]) risk.max_price_deviation_pct = node["max_price_deviation_pct"].as<double>();
    if (node["max_orders_per_second"])   risk.max_orders_per_second = node["max_orders_per_second"].as<int32_t>();
    if (node["max_position_per_session"]) risk.max_position_per_session = node["max_position_per_session"].as<int64_t>();
    return risk;
}

SessionConfig parseSession(const YAML::Node& node) {
    SessionConfig sess;
    if (!node || !node.IsMap()) return sess;
    if (node["hmac_enabled"])           sess.hmac_enabled = node["hmac_enabled"].as<bool>();
    if (node["hmac_key"])               sess.hmac_key = node["hmac_key"].as<std::string>();
    if (node["keep_alive_interval_ms"]) sess.keep_alive_interval_ms = node["keep_alive_interval_ms"].as<uint32_t>();
    if (node["max_sessions"])           sess.max_sessions = node["max_sessions"].as<int>();
    if (node["retransmit_buffer_size"]) sess.retransmit_buffer_size = node["retransmit_buffer_size"].as<int>();
    return sess;
}

ExchangeConfig createDefaultConfig() {
    ExchangeConfig config;
    config.channels = createDefaultChannels();
    config.instruments = createDefaultInstruments();
    return config;
}

} // anonymous namespace

void validateConfig(const ExchangeConfig& config) {
    // Validate network
    if (config.network.tcp_listen_port == 0) {
        throw ConfigValidationError("TCP listen port must be non-zero");
    }
    if (config.network.io_threads < 1) {
        throw ConfigValidationError("io_threads must be at least 1");
    }

    // Validate engine mode
    if (config.engine.mode != "full_matching" && config.engine.mode != "synthetic") {
        throw ConfigValidationError("engine.mode must be 'full_matching' or 'synthetic', got: " + config.engine.mode);
    }
    if (config.engine.mode == "synthetic" && config.engine.pcap_path.empty()) {
        throw ConfigValidationError("pcap_path is required when engine mode is 'synthetic'");
    }
    if (config.engine.synthetic_fill_probability < 0.0 || config.engine.synthetic_fill_probability > 1.0) {
        throw ConfigValidationError("synthetic_fill_probability must be between 0.0 and 1.0");
    }

    // Validate risk limits
    if (config.risk.max_order_qty <= 0) {
        throw ConfigValidationError("max_order_qty must be positive");
    }
    if (config.risk.max_price_deviation_pct <= 0.0) {
        throw ConfigValidationError("max_price_deviation_pct must be positive");
    }
    if (config.risk.max_orders_per_second <= 0) {
        throw ConfigValidationError("max_orders_per_second must be positive");
    }

    // Validate session config
    if (config.session.max_sessions <= 0) {
        throw ConfigValidationError("max_sessions must be positive");
    }
    if (config.session.retransmit_buffer_size <= 0) {
        throw ConfigValidationError("retransmit_buffer_size must be positive");
    }

    // Validate channels
    std::set<int> channel_ids;
    for (const auto& ch : config.channels) {
        if (ch.channel_id <= 0) {
            throw ConfigValidationError("channel_id must be positive");
        }
        if (!channel_ids.insert(ch.channel_id).second) {
            throw ConfigValidationError("Duplicate channel_id: " + std::to_string(ch.channel_id));
        }
        if (ch.symbols.empty()) {
            throw ConfigValidationError("Channel " + std::to_string(ch.channel_id) + " has no symbols");
        }
    }

    // Validate instruments
    std::set<int32_t> security_ids;
    std::set<std::string> symbols;
    for (const auto& inst : config.instruments) {
        if (inst.symbol.empty()) {
            throw ConfigValidationError("Instrument symbol cannot be empty");
        }
        if (!symbols.insert(inst.symbol).second) {
            throw ConfigValidationError("Duplicate instrument symbol: " + inst.symbol);
        }
        if (inst.security_id <= 0) {
            throw ConfigValidationError("security_id must be positive for " + inst.symbol);
        }
        if (!security_ids.insert(inst.security_id).second) {
            throw ConfigValidationError("Duplicate security_id: " + std::to_string(inst.security_id));
        }
        if (inst.tick_size <= 0.0) {
            throw ConfigValidationError("tick_size must be positive for " + inst.symbol);
        }
        if (inst.contract_multiplier <= 0.0) {
            throw ConfigValidationError("contract_multiplier must be positive for " + inst.symbol);
        }
        if (inst.min_trade_vol <= 0) {
            throw ConfigValidationError("min_trade_vol must be positive for " + inst.symbol);
        }
        if (inst.max_trade_vol < inst.min_trade_vol) {
            throw ConfigValidationError("max_trade_vol must be >= min_trade_vol for " + inst.symbol);
        }
        // Verify instrument's channel_id exists
        if (channel_ids.find(inst.channel_id) == channel_ids.end() && !config.channels.empty()) {
            throw ConfigValidationError("Instrument " + inst.symbol + " references unknown channel_id " +
                                        std::to_string(inst.channel_id));
        }
    }

    // Validate log level
    static const std::set<std::string> valid_levels = {"trace", "debug", "info", "warn", "error", "critical", "off"};
    if (valid_levels.find(config.log_level) == valid_levels.end()) {
        throw ConfigValidationError("Invalid log_level: " + config.log_level +
                                    ". Must be one of: trace, debug, info, warn, error, critical, off");
    }
}

ExchangeConfig loadConfig(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return createDefaultConfig();
    }

    YAML::Node root = YAML::LoadFile(path);
    ExchangeConfig config;

    if (root["network"]) {
        config.network = parseNetwork(root["network"]);
    }

    if (root["engine"]) {
        config.engine = parseEngine(root["engine"]);
    }

    if (root["risk"]) {
        config.risk = parseRisk(root["risk"]);
    }

    if (root["session"]) {
        config.session = parseSession(root["session"]);
    }

    if (root["log_level"]) {
        config.log_level = root["log_level"].as<std::string>();
    }

    // Parse channels
    if (root["channels"] && root["channels"].IsSequence()) {
        for (const auto& node : root["channels"]) {
            config.channels.push_back(parseChannel(node));
        }
    } else {
        config.channels = createDefaultChannels();
    }

    // Parse instruments
    if (root["instruments"] && root["instruments"].IsSequence()) {
        for (const auto& node : root["instruments"]) {
            config.instruments.push_back(parseInstrument(node));
        }
    } else {
        config.instruments = createDefaultInstruments();
    }

    validateConfig(config);
    return config;
}

} // namespace cme::sim::config
