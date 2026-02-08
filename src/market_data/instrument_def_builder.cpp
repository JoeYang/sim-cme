#include "market_data/instrument_def_builder.h"
#include "../sbe/mdp3_messages.h"
#include <algorithm>
#include <cstring>

namespace cme::sim::market_data {

std::vector<uint8_t> InstrumentDefBuilder::buildDefinition(
    const Instrument& instrument,
    uint32_t tot_num_reports) {

    sbe::MDInstrumentDefinitionFuture54 msg{};

    msg.matchEventIndicator = static_cast<uint8_t>(MatchEventIndicator::EndOfEvent);
    msg.totNumReports = tot_num_reports;
    msg.securityUpdateAction = 'A'; // Add
    msg.lastUpdateTime = 0;
    msg.mdSecurityTradingStatus =
        static_cast<uint8_t>(instrument.trading_status);
    msg.applID = static_cast<int16_t>(instrument.channel_id);
    msg.marketSegmentID = static_cast<uint8_t>(instrument.channel_id);
    msg.underlyingProduct = 14; // Future

    // Copy fixed-length string fields with zero-fill
    auto copyStr = [](char* dst, size_t dst_len, const std::string& src) {
        std::memset(dst, 0, dst_len);
        std::memcpy(dst, src.data(), std::min(src.size(), dst_len));
    };

    copyStr(msg.securityExchange, 4, "XCME");
    copyStr(msg.securityGroup, 6, instrument.security_group);
    copyStr(msg.asset, 6, instrument.asset);
    copyStr(msg.symbol, 20, instrument.symbol);
    msg.securityID = instrument.security_id;
    copyStr(msg.securityType, 6, "FUT");
    copyStr(msg.cfiCode, 6, "FXXXXX");

    // MaturityMonthYear encoding: first 2 bytes = year, 1 byte = month
    if (instrument.maturity_month_year.size() >= 6) {
        uint16_t year = static_cast<uint16_t>(
            std::stoi(instrument.maturity_month_year.substr(0, 4)));
        uint8_t month = static_cast<uint8_t>(
            std::stoi(instrument.maturity_month_year.substr(4, 2)));
        std::memcpy(msg.maturityMonthYear, &year, 2);
        msg.maturityMonthYear[2] = static_cast<char>(month);
        msg.maturityMonthYear[3] = 0; // day (null)
        msg.maturityMonthYear[4] = 0; // week (null)
    }

    copyStr(msg.currency, 3, "USD");
    copyStr(msg.settlCurrency, 3, "USD");
    msg.matchAlgorithm = 'F'; // FIFO

    msg.minTradeVol = static_cast<uint32_t>(instrument.min_trade_vol);
    msg.maxTradeVol = static_cast<uint32_t>(instrument.max_trade_vol);

    // Price fields in PRICENULL9 fixed-point format (mantissa * 10^-9)
    msg.minPriceIncrement =
        static_cast<int64_t>(instrument.tick_size * 1e9);
    msg.displayFactor =
        static_cast<int64_t>(instrument.display_factor * 1e9);
    msg.minPriceIncrementAmount =
        static_cast<int64_t>(instrument.min_price_increment_amount * 1e9);

    msg.contractMultiplier =
        static_cast<int32_t>(instrument.contract_multiplier);
    msg.originalContractSize =
        static_cast<int32_t>(instrument.contract_multiplier);

    copyStr(msg.unitOfMeasure, 30, instrument.unit_of_measure);
    msg.unitOfMeasureQty =
        static_cast<int64_t>(instrument.contract_multiplier * 1e9);

    msg.tradingReferencePrice = Price::null().mantissa;
    msg.highLimitPrice = Price::null().mantissa;
    msg.lowLimitPrice = Price::null().mantissa;
    msg.maxPriceVariation = Price::null().mantissa;
    msg.userDefinedInstrument = 'N';

    std::vector<uint8_t> buf(msg.encodedLength());
    msg.encode(reinterpret_cast<char*>(buf.data()), 0);
    return buf;
}

} // namespace cme::sim::market_data
