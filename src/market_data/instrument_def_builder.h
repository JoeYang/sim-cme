#pragma once

#include "../instruments/instrument.h"
#include <cstdint>
#include <vector>

namespace cme::sim::market_data {

/// Builds MDInstrumentDefinitionFuture54 messages from Instrument metadata.
class InstrumentDefBuilder {
public:
    /// Build a full instrument definition message.
    /// @param instrument  The instrument to encode
    /// @param tot_num_reports  Total number of instrument defs in this cycle
    std::vector<uint8_t> buildDefinition(const Instrument& instrument,
                                          uint32_t tot_num_reports = 0);
};

} // namespace cme::sim::market_data
