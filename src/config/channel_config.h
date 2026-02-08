#pragma once
#include "exchange_config.h"
#include <vector>

namespace cme::sim::config {

// Create default channel configurations for CME E-mini futures channels 310-313.
// Feed A uses multicast addresses 239.1.1.1 / .2 / .3
// Feed B uses multicast addresses 239.1.1.4 / .5 / .6
inline std::vector<ChannelConfig> createDefaultChannels() {
    std::vector<ChannelConfig> channels;

    // Channel 310: E-mini S&P 500 (ES) and Micro E-mini S&P 500 (MES)
    {
        ChannelConfig ch;
        ch.channel_id = 310;
        ch.name = "ES/MES";
        ch.incremental_feed = {"239.1.1.1", 14310, "239.1.1.4", 14310};
        ch.snapshot_feed    = {"239.1.1.2", 15310, "239.1.1.5", 15310};
        ch.instrument_def_feed = {"239.1.1.3", 16310, "239.1.1.6", 16310};
        ch.symbols = {"ESH5", "ESM5", "MESH5", "MESM5"};
        channels.push_back(ch);
    }

    // Channel 311: E-mini NASDAQ-100 (NQ) and Micro E-mini NASDAQ-100 (MNQ)
    {
        ChannelConfig ch;
        ch.channel_id = 311;
        ch.name = "NQ/MNQ";
        ch.incremental_feed = {"239.1.1.1", 14311, "239.1.1.4", 14311};
        ch.snapshot_feed    = {"239.1.1.2", 15311, "239.1.1.5", 15311};
        ch.instrument_def_feed = {"239.1.1.3", 16311, "239.1.1.6", 16311};
        ch.symbols = {"NQH5", "NQM5", "MNQH5", "MNQM5"};
        channels.push_back(ch);
    }

    // Channel 312: E-mini Dow (YM) and Micro E-mini Dow (MYM)
    {
        ChannelConfig ch;
        ch.channel_id = 312;
        ch.name = "YM/MYM";
        ch.incremental_feed = {"239.1.1.1", 14312, "239.1.1.4", 14312};
        ch.snapshot_feed    = {"239.1.1.2", 15312, "239.1.1.5", 15312};
        ch.instrument_def_feed = {"239.1.1.3", 16312, "239.1.1.6", 16312};
        ch.symbols = {"YMH5", "YMM5", "MYMH5", "MYMM5"};
        channels.push_back(ch);
    }

    // Channel 313: E-mini Russell 2000 (RTY) and Micro E-mini Russell 2000 (M2K)
    {
        ChannelConfig ch;
        ch.channel_id = 313;
        ch.name = "RTY/M2K";
        ch.incremental_feed = {"239.1.1.1", 14313, "239.1.1.4", 14313};
        ch.snapshot_feed    = {"239.1.1.2", 15313, "239.1.1.5", 15313};
        ch.instrument_def_feed = {"239.1.1.3", 16313, "239.1.1.6", 16313};
        ch.symbols = {"RTYH5", "RTYM5", "M2KH5", "M2KM5"};
        channels.push_back(ch);
    }

    return channels;
}

// Create default instrument configurations for all 16 instruments (front + back month)
inline std::vector<InstrumentConfig> createDefaultInstruments() {
    std::vector<InstrumentConfig> instruments;

    // Channel 310: ES
    instruments.push_back({"ESH5",  1, 310, 0.25, 50.0, 12.50, 1, 10000, "202503", 0.01});
    instruments.push_back({"ESM5",  2, 310, 0.25, 50.0, 12.50, 1, 10000, "202506", 0.01});

    // Channel 310: MES
    instruments.push_back({"MESH5", 3, 310, 0.25,  5.0,  1.25, 1, 10000, "202503", 0.01});
    instruments.push_back({"MESM5", 4, 310, 0.25,  5.0,  1.25, 1, 10000, "202506", 0.01});

    // Channel 311: NQ
    instruments.push_back({"NQH5",  5, 311, 0.25, 20.0,  5.00, 1, 10000, "202503", 0.01});
    instruments.push_back({"NQM5",  6, 311, 0.25, 20.0,  5.00, 1, 10000, "202506", 0.01});

    // Channel 311: MNQ
    instruments.push_back({"MNQH5", 7, 311, 0.25,  2.0,  0.50, 1, 10000, "202503", 0.01});
    instruments.push_back({"MNQM5", 8, 311, 0.25,  2.0,  0.50, 1, 10000, "202506", 0.01});

    // Channel 312: YM
    instruments.push_back({"YMH5",  9, 312, 1.00,  5.0,  5.00, 1, 10000, "202503", 0.01});
    instruments.push_back({"YMM5", 10, 312, 1.00,  5.0,  5.00, 1, 10000, "202506", 0.01});

    // Channel 312: MYM
    instruments.push_back({"MYMH5", 11, 312, 1.00,  0.5,  0.50, 1, 10000, "202503", 0.01});
    instruments.push_back({"MYMM5", 12, 312, 1.00,  0.5,  0.50, 1, 10000, "202506", 0.01});

    // Channel 313: RTY
    instruments.push_back({"RTYH5", 13, 313, 0.10, 50.0,  5.00, 1, 10000, "202503", 0.01});
    instruments.push_back({"RTYM5", 14, 313, 0.10, 50.0,  5.00, 1, 10000, "202506", 0.01});

    // Channel 313: M2K
    instruments.push_back({"M2KH5", 15, 313, 0.10,  5.0,  0.50, 1, 10000, "202503", 0.01});
    instruments.push_back({"M2KM5", 16, 313, 0.10,  5.0,  0.50, 1, 10000, "202506", 0.01});

    return instruments;
}

} // namespace cme::sim::config
