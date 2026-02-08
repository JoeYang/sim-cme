# sim-cme-exchange

A simulated CME Group exchange for testing trading systems against realistic iLink 3 order entry and MDP 3.0 market data interfaces. Implements the same wire protocols used by CME Globex so clients can develop and test without connecting to production or certification environments.

## What It Does

- **Order entry** via iLink 3 over TCP (FIXP session layer + SBE-encoded messages)
- **Market data** via MDP 3.0 over UDP multicast (incremental, snapshot, and instrument definition feeds)
- **Full matching engine** with price-time priority across 16 futures instruments
- **Pre-trade risk checks** including order size limits, price deviation, rate throttling, and position limits

## Instruments

4 channels covering the major CME equity index futures and their micro variants:

| Channel | Products | Symbols |
|---------|----------|---------|
| 310 | E-mini / Micro S&P 500 | ESH5, ESM5, MESH5, MESM5 |
| 311 | E-mini / Micro NASDAQ-100 | NQH5, NQM5, MNQH5, MNQM5 |
| 312 | E-mini / Micro Dow | YMH5, YMM5, MYMH5, MYMM5 |
| 313 | E-mini / Micro Russell 2000 | RTYH5, RTYM5, M2KH5, M2KM5 |

## Architecture

```
                    TCP :9563
                       |
               +-------v--------+
               |  FIXP Session  |    iLink 3 (Negotiate/Establish/Terminate)
               |    Manager     |    SBE-encoded order messages
               +-------+--------+
                       |
               +-------v--------+
               | Order Entry    |    Decode SBE, validate, risk check
               |   Gateway      |
               +-------+--------+
                       |
               +-------v--------+
               | Matching       |    Price-time priority order book
               |   Engine       |    Generates fills + book updates
               +---+-------+----+
                   |       |
          +--------v--+ +--v-----------+
          | Exec      | | Market Data  |    MDP 3.0 SBE messages
          | Reports   | | Publisher    |    UDP multicast
          | (TCP)     | | (multicast)  |
          +-----------+ +--------------+
```

**Threading model**: Single-threaded engine (no locks on the hot path), MPSC queue for inbound orders from IO threads, responses routed back to sessions via callbacks.

## Building

Requires Bazel 9+ and a C++20 compiler (GCC 11+ or Clang 14+).

```bash
# Build everything
bazel build //...

# Build optimized
bazel build --config=opt //src:sim_cme_exchange

# Run tests
bazel test //tests:unit_tests //tests:integration_tests

# Sanitizers
bazel build --config=asan //...
bazel build --config=tsan //...
```

## Running

```bash
# Start the exchange
./start_exchange.sh start

# Check status
./start_exchange.sh status

# View logs
./start_exchange.sh log

# Stop
./start_exchange.sh stop
```

Or run directly:

```bash
bazel run --config=opt //src:sim_cme_exchange -- --config config/exchange_config.yaml
```

## Connectivity

### Order Entry (iLink 3 over TCP)

| Parameter | Value |
|-----------|-------|
| Host | `0.0.0.0` |
| Port | `9563` |
| Protocol | FIXP + SBE |
| Framing | SOFH (Simple Open Framing Header) |

Supported messages:
- `Negotiate500` / `NegotiationResponse501`
- `Establish503` / `EstablishmentAck504`
- `NewOrderSingle514` / `ExecutionReportNew522`
- `OrderCancelReplaceRequest515` / `ExecutionReportModify531`
- `OrderCancelRequest516` / `ExecutionReportCancel534`
- `ExecutionReportTradeOutright525` (fills)
- `ExecutionReportReject523`, `OrderCancelReject535`
- `Sequence506` (heartbeats), `Terminate507`
- `RetransmitRequest508` / `Retransmission509`

### Market Data (MDP 3.0 over UDP Multicast)

Each channel has 3 feed types with A/B redundancy:

| Channel | Incremental (A) | Snapshot (A) | Instrument Def (A) |
|---------|-----------------|--------------|---------------------|
| 310 | `239.1.1.1:14310` | `239.1.1.2:15310` | `239.1.1.3:16310` |
| 311 | `239.1.1.1:14311` | `239.1.1.2:15311` | `239.1.1.3:16311` |
| 312 | `239.1.1.1:14312` | `239.1.1.2:15312` | `239.1.1.3:16312` |
| 313 | `239.1.1.1:14313` | `239.1.1.2:15313` | `239.1.1.3:16313` |

Feed B uses addresses `239.1.1.4` / `239.1.1.5` / `239.1.1.6` on the same ports.

MDP 3.0 message types:
- `MDIncrementalRefreshBook46` (book updates)
- `MDIncrementalRefreshTradeSummary48` (trade summaries)
- `SnapshotFullRefresh52` (periodic book snapshots)
- `MDInstrumentDefinitionFutures54` (instrument definitions, replayed on loop)
- `SecurityStatus30` (trading status changes)
- `ChannelReset4`

## Test Tools

### ilink3_client

Interactive or automated test client for sending orders:

```bash
# Interactive mode
bazel-bin/tools/ilink3_client --host 127.0.0.1 --port 9563 --interactive

# Auto mode: send N orders
bazel-bin/tools/ilink3_client --host 127.0.0.1 --port 9563 --auto 50
```

Interactive commands: `buy`, `sell`, `cancel`, `modify`, `status`, `orders`, `quit`.

### mdp3_listener

Multicast market data listener for debugging:

```bash
bazel-bin/tools/mdp3_listener --group 239.1.1.1 --port 14310
```

### send_orders.sh

Script to generate sustained order flow with fills:

```bash
./send_orders.sh --rounds 20 --pace 1
```

## Configuration

All settings are in `config/exchange_config.yaml`:

- **Network**: TCP listen address/port, multicast addresses, IO thread count
- **Engine**: Full matching or synthetic mode (for replay testing)
- **Risk**: Max order qty, price deviation %, rate limits, position limits
- **Session**: HMAC auth, keepalive interval, max sessions, retransmit buffer size
- **Channels**: Multicast feed addresses and instrument assignments
- **Instruments**: Symbol, security ID, tick size, contract multiplier, maturity

## Project Structure

```
src/
  common/          Shared types, clock, endian utils, logger, queues
  config/          YAML config loader
  sbe/             Hand-crafted SBE codecs for iLink 3 and MDP 3.0
  engine/          Order book, matching engine, price levels
  fixp/            FIXP session state machine, session manager
  gateway/         Order entry gateway, exec report builder, risk manager
  instruments/     Instrument manager, security status machine
  network/         TCP acceptor/connection, UDP multicast sender
  market_data/     Channel publisher, incremental/snapshot builders
  main.cpp         Application orchestrator
tests/
  unit/            Order book, FIXP session, SBE codec, instrument tests
  integration/     FIXP lifecycle, order-to-market-data, snapshot recovery
tools/
  ilink3_client    Test order entry client
  mdp3_listener    Multicast market data listener
config/
  exchange_config.yaml
```
