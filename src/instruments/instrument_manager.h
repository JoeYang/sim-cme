#pragma once

#include "instrument.h"
#include "channel.h"
#include "../config/exchange_config.h"
#include <vector>
#include <unordered_map>
#include <string>

namespace cme::sim {

class InstrumentManager {
public:
    // Load instruments and channels from configuration
    void loadFromConfig(const std::vector<config::InstrumentConfig>& instrument_configs,
                        const std::vector<config::ChannelConfig>& channel_configs);

    // Instrument lookups
    const Instrument* findBySecurityId(SecurityId id) const;
    const Instrument* findBySymbol(const std::string& symbol) const;
    std::vector<const Instrument*> getInstrumentsByChannel(int channel_id) const;
    const std::vector<Instrument>& getAllInstruments() const;

    // Channel lookups
    const Channel* findChannel(int channel_id) const;
    const std::vector<Channel>& getAllChannels() const;

    // Trading status management
    void setTradingStatus(SecurityId id, SecurityTradingStatus status);

private:
    std::vector<Instrument> instruments_;
    std::unordered_map<SecurityId, size_t> by_security_id_;
    std::unordered_map<std::string, size_t> by_symbol_;

    std::vector<Channel> channels_;
    std::unordered_map<int, size_t> channels_by_id_;

    // Derive security group / asset from symbol (e.g. "ESH5" -> "ES")
    static std::string deriveSecurityGroup(const std::string& symbol);
};

} // namespace cme::sim
