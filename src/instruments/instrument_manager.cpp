#include "instrument_manager.h"
#include <algorithm>
#include <cctype>

namespace cme::sim {

void InstrumentManager::loadFromConfig(
        const std::vector<config::InstrumentConfig>& instrument_configs,
        const std::vector<config::ChannelConfig>& channel_configs) {

    instruments_.clear();
    by_security_id_.clear();
    by_symbol_.clear();
    channels_.clear();
    channels_by_id_.clear();

    // Load channels first
    for (const auto& cc : channel_configs) {
        Channel ch;
        ch.channel_id = cc.channel_id;
        ch.name = cc.name;
        ch.incremental_feed = cc.incremental_feed;
        ch.snapshot_feed = cc.snapshot_feed;
        ch.instrument_def_feed = cc.instrument_def_feed;
        // security_ids will be populated when instruments are loaded

        size_t idx = channels_.size();
        channels_.push_back(std::move(ch));
        channels_by_id_[cc.channel_id] = idx;
    }

    // Load instruments
    for (const auto& ic : instrument_configs) {
        Instrument inst;
        inst.security_id = ic.security_id;
        inst.symbol = ic.symbol;
        inst.security_group = deriveSecurityGroup(ic.symbol);
        inst.asset = inst.security_group;
        inst.channel_id = ic.channel_id;
        inst.tick_size = ic.tick_size;
        inst.contract_multiplier = ic.contract_multiplier;
        inst.min_price_increment_amount = ic.min_price_increment_amount;
        inst.display_factor = ic.display_factor;
        inst.min_trade_vol = ic.min_trade_vol;
        inst.max_trade_vol = ic.max_trade_vol;
        inst.maturity_month_year = ic.maturity_month_year;
        inst.trading_status = SecurityTradingStatus::PreOpen;

        size_t idx = instruments_.size();
        by_security_id_[inst.security_id] = idx;
        by_symbol_[inst.symbol] = idx;

        // Associate with channel
        auto it = channels_by_id_.find(ic.channel_id);
        if (it != channels_by_id_.end()) {
            channels_[it->second].security_ids.push_back(inst.security_id);
        }

        instruments_.push_back(std::move(inst));
    }
}

const Instrument* InstrumentManager::findBySecurityId(SecurityId id) const {
    auto it = by_security_id_.find(id);
    if (it == by_security_id_.end()) return nullptr;
    return &instruments_[it->second];
}

const Instrument* InstrumentManager::findBySymbol(const std::string& symbol) const {
    auto it = by_symbol_.find(symbol);
    if (it == by_symbol_.end()) return nullptr;
    return &instruments_[it->second];
}

std::vector<const Instrument*> InstrumentManager::getInstrumentsByChannel(int channel_id) const {
    std::vector<const Instrument*> result;
    for (const auto& inst : instruments_) {
        if (inst.channel_id == channel_id) {
            result.push_back(&inst);
        }
    }
    return result;
}

const std::vector<Instrument>& InstrumentManager::getAllInstruments() const {
    return instruments_;
}

const Channel* InstrumentManager::findChannel(int channel_id) const {
    auto it = channels_by_id_.find(channel_id);
    if (it == channels_by_id_.end()) return nullptr;
    return &channels_[it->second];
}

const std::vector<Channel>& InstrumentManager::getAllChannels() const {
    return channels_;
}

void InstrumentManager::setTradingStatus(SecurityId id, SecurityTradingStatus status) {
    auto it = by_security_id_.find(id);
    if (it != by_security_id_.end()) {
        instruments_[it->second].trading_status = status;
    }
}

std::string InstrumentManager::deriveSecurityGroup(const std::string& symbol) {
    // CME futures symbols: root (letters) + month code (1 letter) + year digit(s)
    // Examples: ESH5, MESH5, NQM5, MNQM5, YMH5, MYMH5, RTYH5, M2KH5
    // We strip trailing month code + year digits to get the root.
    if (symbol.empty()) return {};

    // Find the last alphabetic character that is followed only by digits.
    // The month code is a single uppercase letter [FGHJKMNQUVXZ].
    // Walk backwards past digits to find the month code.
    size_t pos = symbol.size();

    // Skip trailing digits (year part)
    while (pos > 0 && std::isdigit(static_cast<unsigned char>(symbol[pos - 1]))) {
        --pos;
    }

    // pos now points right after the month code. Skip the month code itself.
    if (pos > 0 && std::isalpha(static_cast<unsigned char>(symbol[pos - 1]))) {
        --pos;
    }

    if (pos == 0) return symbol; // fallback
    return symbol.substr(0, pos);
}

} // namespace cme::sim
