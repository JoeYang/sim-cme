#include <gtest/gtest.h>
#include "instruments/instrument_manager.h"
#include "instruments/instrument.h"
#include "config/exchange_config.h"
#include "common/types.h"
#include <vector>

using namespace cme::sim;
using namespace cme::sim::config;

class InstrumentManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up channel configs
        ChannelConfig ch310;
        ch310.channel_id = 310;
        ch310.name = "Channel 310 - ES/MES";
        ch310.incremental_feed.address_a = "239.1.1.1";
        ch310.incremental_feed.port_a = 14310;
        ch310.snapshot_feed.address_a = "239.1.1.2";
        ch310.snapshot_feed.port_a = 14311;
        ch310.instrument_def_feed.address_a = "239.1.1.3";
        ch310.instrument_def_feed.port_a = 14312;
        channel_configs_.push_back(ch310);

        ChannelConfig ch320;
        ch320.channel_id = 320;
        ch320.name = "Channel 320 - NQ/MNQ";
        ch320.incremental_feed.address_a = "239.1.1.10";
        ch320.incremental_feed.port_a = 14320;
        channel_configs_.push_back(ch320);

        // Set up instrument configs
        InstrumentConfig es;
        es.symbol = "ESH5";
        es.security_id = 1001;
        es.channel_id = 310;
        es.tick_size = 0.25;
        es.contract_multiplier = 50.0;
        es.min_price_increment_amount = 12.50;
        es.display_factor = 0.01;
        es.min_trade_vol = 1;
        es.max_trade_vol = 10000;
        es.maturity_month_year = "202503";
        instrument_configs_.push_back(es);

        InstrumentConfig mes;
        mes.symbol = "MESH5";
        mes.security_id = 1002;
        mes.channel_id = 310;
        mes.tick_size = 0.25;
        mes.contract_multiplier = 5.0;
        mes.min_price_increment_amount = 1.25;
        mes.display_factor = 0.01;
        mes.maturity_month_year = "202503";
        instrument_configs_.push_back(mes);

        InstrumentConfig nq;
        nq.symbol = "NQM5";
        nq.security_id = 2001;
        nq.channel_id = 320;
        nq.tick_size = 0.25;
        nq.contract_multiplier = 20.0;
        nq.min_price_increment_amount = 5.0;
        nq.display_factor = 0.01;
        nq.maturity_month_year = "202506";
        instrument_configs_.push_back(nq);

        mgr_.loadFromConfig(instrument_configs_, channel_configs_);
    }

    InstrumentManager mgr_;
    std::vector<InstrumentConfig> instrument_configs_;
    std::vector<ChannelConfig> channel_configs_;
};

// ---------------------------------------------------------------------------
// 1. LoadInstruments
// ---------------------------------------------------------------------------
TEST_F(InstrumentManagerTest, LoadInstruments) {
    EXPECT_EQ(mgr_.getAllInstruments().size(), 3u);
    EXPECT_EQ(mgr_.getAllChannels().size(), 2u);
}

// ---------------------------------------------------------------------------
// 2. FindBySecurityId
// ---------------------------------------------------------------------------
TEST_F(InstrumentManagerTest, FindBySecurityId) {
    const Instrument* inst = mgr_.findBySecurityId(1001);
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->symbol, "ESH5");
    EXPECT_EQ(inst->security_id, 1001);
    EXPECT_EQ(inst->channel_id, 310);

    // Non-existent
    EXPECT_EQ(mgr_.findBySecurityId(9999), nullptr);
}

// ---------------------------------------------------------------------------
// 3. FindBySymbol
// ---------------------------------------------------------------------------
TEST_F(InstrumentManagerTest, FindBySymbol) {
    const Instrument* inst = mgr_.findBySymbol("MESH5");
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->security_id, 1002);

    // Non-existent
    EXPECT_EQ(mgr_.findBySymbol("INVALID"), nullptr);
}

// ---------------------------------------------------------------------------
// 4. GetByChannel
// ---------------------------------------------------------------------------
TEST_F(InstrumentManagerTest, GetByChannel) {
    auto ch310_insts = mgr_.getInstrumentsByChannel(310);
    EXPECT_EQ(ch310_insts.size(), 2u); // ES and MES

    auto ch320_insts = mgr_.getInstrumentsByChannel(320);
    EXPECT_EQ(ch320_insts.size(), 1u); // NQ

    auto ch999_insts = mgr_.getInstrumentsByChannel(999);
    EXPECT_EQ(ch999_insts.size(), 0u);
}

// ---------------------------------------------------------------------------
// 5. TickValidation
// ---------------------------------------------------------------------------
TEST_F(InstrumentManagerTest, TickValidation) {
    const Instrument* es = mgr_.findBySymbol("ESH5");
    ASSERT_NE(es, nullptr);

    // Valid tick: 5000.25 (on 0.25 boundary)
    EXPECT_TRUE(es->isValidTick(Price::fromDouble(5000.25)));
    EXPECT_TRUE(es->isValidTick(Price::fromDouble(5000.50)));
    EXPECT_TRUE(es->isValidTick(Price::fromDouble(5000.75)));
    EXPECT_TRUE(es->isValidTick(Price::fromDouble(5000.00)));

    // Invalid tick: 5000.10 (not on 0.25 boundary)
    EXPECT_FALSE(es->isValidTick(Price::fromDouble(5000.10)));
    EXPECT_FALSE(es->isValidTick(Price::fromDouble(5000.33)));
}

// ---------------------------------------------------------------------------
// 6. PriceRounding
// ---------------------------------------------------------------------------
TEST_F(InstrumentManagerTest, PriceRounding) {
    const Instrument* es = mgr_.findBySymbol("ESH5");
    ASSERT_NE(es, nullptr);

    // 5000.10 should round to 5000.00 (closest tick)
    Price rounded = es->roundToTick(Price::fromDouble(5000.10));
    EXPECT_EQ(rounded, Price::fromDouble(5000.00));

    // 5000.13 should round to 5000.25 (closer to 5000.25 than 5000.00)
    rounded = es->roundToTick(Price::fromDouble(5000.13));
    EXPECT_EQ(rounded, Price::fromDouble(5000.25));

    // Already on tick
    rounded = es->roundToTick(Price::fromDouble(5000.50));
    EXPECT_EQ(rounded, Price::fromDouble(5000.50));
}

// ---------------------------------------------------------------------------
// 7. TickConversions
// ---------------------------------------------------------------------------
TEST_F(InstrumentManagerTest, TickConversions) {
    const Instrument* es = mgr_.findBySymbol("ESH5");
    ASSERT_NE(es, nullptr);

    // 4 ticks = 1.00 for ES (tick=0.25)
    Price four_ticks = es->ticksToPrice(4);
    EXPECT_NEAR(four_ticks.toDouble(), 1.0, 1e-6);

    // 1.00 = 4 ticks
    int64_t ticks = es->priceToTicks(Price::fromDouble(1.0));
    EXPECT_EQ(ticks, 4);
}

// ---------------------------------------------------------------------------
// 8. ChannelLookup
// ---------------------------------------------------------------------------
TEST_F(InstrumentManagerTest, ChannelLookup) {
    const Channel* ch = mgr_.findChannel(310);
    ASSERT_NE(ch, nullptr);
    EXPECT_EQ(ch->channel_id, 310);
    EXPECT_EQ(ch->name, "Channel 310 - ES/MES");
    EXPECT_EQ(ch->security_ids.size(), 2u);

    EXPECT_EQ(mgr_.findChannel(999), nullptr);
}

// ---------------------------------------------------------------------------
// 9. TradingStatusManagement
// ---------------------------------------------------------------------------
TEST_F(InstrumentManagerTest, TradingStatusManagement) {
    const Instrument* es = mgr_.findBySecurityId(1001);
    ASSERT_NE(es, nullptr);
    EXPECT_EQ(es->trading_status, SecurityTradingStatus::PreOpen);

    mgr_.setTradingStatus(1001, SecurityTradingStatus::Open);
    es = mgr_.findBySecurityId(1001);
    EXPECT_EQ(es->trading_status, SecurityTradingStatus::Open);

    mgr_.setTradingStatus(1001, SecurityTradingStatus::Halt);
    es = mgr_.findBySecurityId(1001);
    EXPECT_EQ(es->trading_status, SecurityTradingStatus::Halt);
}

// ---------------------------------------------------------------------------
// 10. SecurityGroupDerivation
// ---------------------------------------------------------------------------
TEST_F(InstrumentManagerTest, SecurityGroupDerivation) {
    // ESH5 -> security_group = "ES"
    const Instrument* es = mgr_.findBySymbol("ESH5");
    ASSERT_NE(es, nullptr);
    EXPECT_EQ(es->security_group, "ES");

    // MESH5 -> security_group = "MES"
    const Instrument* mes = mgr_.findBySymbol("MESH5");
    ASSERT_NE(mes, nullptr);
    EXPECT_EQ(mes->security_group, "MES");

    // NQM5 -> security_group = "NQ"
    const Instrument* nq = mgr_.findBySymbol("NQM5");
    ASSERT_NE(nq, nullptr);
    EXPECT_EQ(nq->security_group, "NQ");
}
