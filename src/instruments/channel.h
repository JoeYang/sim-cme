#pragma once

#include "../common/types.h"
#include "../config/exchange_config.h"
#include <string>
#include <vector>

namespace cme::sim {

struct Channel {
    int channel_id;
    std::string name;                          // e.g. "Channel 310 - ES/MES"
    std::vector<SecurityId> security_ids;
    config::FeedConfig incremental_feed;
    config::FeedConfig snapshot_feed;
    config::FeedConfig instrument_def_feed;
};

} // namespace cme::sim
