// mdp3_listener - MDP 3.0 multicast listener and decoder
// Usage: mdp3_listener --channel CHANNEL_ID [--feed incremental|snapshot|instdef]
//                      [--group MULTICAST_ADDR] [--port PORT] [--iface INTERFACE]

#ifdef ASIO_STANDALONE
#include <asio.hpp>
namespace boost {
    namespace asio = ::asio;
    namespace system { using ::asio::error_code; }
}
#else
#include <boost/asio.hpp>
#endif
#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iomanip>

#include "sbe/mdp3_messages.h"
#include "sbe/ilink3_messages.h"
#include "sbe/packet_header.h"
#include "sbe/message_header.h"
#include "common/types.h"

using boost::asio::ip::udp;
using boost::asio::ip::address;
namespace asio = boost::asio;
using namespace cme::sim::sbe;
using namespace cme::sim;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static std::atomic<bool> g_running{true};

static void signalHandler(int) { g_running = false; }

// ---------------------------------------------------------------------------
// Feed type enumeration
// ---------------------------------------------------------------------------
enum class FeedType {
    Incremental,
    Snapshot,
    InstrumentDef
};

// ---------------------------------------------------------------------------
// Default multicast addresses per channel
// ---------------------------------------------------------------------------
struct ChannelConfig {
    std::string incrementalAddr;
    uint16_t incrementalPort;
    std::string snapshotAddr;
    uint16_t snapshotPort;
    std::string instdefAddr;
    uint16_t instdefPort;
};

static ChannelConfig getDefaultChannel(int channelId) {
    // Default multicast addresses follow a pattern:
    //   Incremental: 239.1.1.1:14XXX
    //   Snapshot:    239.1.1.2:15XXX
    //   InstrDef:    239.1.1.3:16XXX
    return {
        "239.1.1.1", static_cast<uint16_t>(14000 + channelId),
        "239.1.1.2", static_cast<uint16_t>(15000 + channelId),
        "239.1.1.3", static_cast<uint16_t>(16000 + channelId)
    };
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------
struct Stats {
    uint64_t packetsReceived = 0;
    uint64_t messagesDecoded = 0;
    uint64_t gaps = 0;
    uint32_t lastSeqNum = 0;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastPrintTime;
};

static void printStats(Stats& stats) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - stats.startTime).count();
    std::cout << "\n--- Statistics (after " << elapsed << "s) ---\n"
              << "  Packets received:  " << stats.packetsReceived << "\n"
              << "  Messages decoded:  " << stats.messagesDecoded << "\n"
              << "  Sequence gaps:     " << stats.gaps << "\n"
              << "  Last SeqNum:       " << stats.lastSeqNum << "\n"
              << std::endl;
    stats.lastPrintTime = now;
}

// ---------------------------------------------------------------------------
// Format a timestamp (nanoseconds since epoch)
// ---------------------------------------------------------------------------
static std::string formatTimestamp(uint64_t nanos) {
    auto secs = nanos / 1'000'000'000ULL;
    auto nsRem = nanos % 1'000'000'000ULL;
    auto tp = std::chrono::system_clock::from_time_t(static_cast<time_t>(secs));
    auto tt = std::chrono::system_clock::to_time_t(tp);
    struct tm tm_buf;
    localtime_r(&tt, &tm_buf);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);
    char result[80];
    std::snprintf(result, sizeof(result), "%s.%09lu", buf,
                  static_cast<unsigned long>(nsRem));
    return result;
}

// ---------------------------------------------------------------------------
// Side/action helpers
// ---------------------------------------------------------------------------
static const char* updateActionStr(uint8_t action) {
    switch (action) {
        case 0: return "New";
        case 1: return "Change";
        case 2: return "Delete";
        case 3: return "DeleteThru";
        case 4: return "DeleteFrom";
        case 5: return "Overlay";
        default: return "Unknown";
    }
}

static const char* entryTypeStr(char et) {
    switch (et) {
        case '0': return "Bid";
        case '1': return "Offer";
        case '2': return "Trade";
        default:  return "?";
    }
}

static const char* aggressorSideStr(uint8_t side) {
    switch (side) {
        case 1: return "Buy";
        case 2: return "Sell";
        default: return "None";
    }
}

static const char* tradingStatusStr(uint8_t status) {
    switch (status) {
        case 2:  return "PreOpen";
        case 17: return "Open";
        case 18: return "Halt";
        case 21: return "Close";
        default: return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// Decode individual SBE messages within a packet
// ---------------------------------------------------------------------------
static void decodeMessage(const char* data, size_t offset, size_t totalLen,
                          Stats& stats) {
    if (offset + MessageHeader::SIZE > totalLen) return;

    uint16_t templateId = MessageHeader::decodeTemplateId(data + offset);
    uint16_t blockLength = MessageHeader::decodeBlockLength(data + offset);
    stats.messagesDecoded++;

    switch (templateId) {
    case MDIncrementalRefreshBook46::TEMPLATE_ID: {
        MDIncrementalRefreshBook46 msg;
        msg.decode(data, offset);
        std::cout << "  [Book46] time=" << formatTimestamp(msg.transactTime)
                  << " entries=" << msg.entries.size() << "\n";
        for (const auto& e : msg.entries) {
            std::cout << "    " << entryTypeStr(e.mdEntryType)
                      << " Action=" << updateActionStr(e.mdUpdateAction)
                      << " SecID=" << e.securityID
                      << " Level=" << static_cast<int>(e.mdPriceLevel)
                      << " Price=" << Price{e.mdEntryPx}.toDouble()
                      << " Size=" << e.mdEntrySize
                      << " Orders=" << e.numberOfOrders
                      << " RptSeq=" << e.rptSeq << "\n";
        }
        break;
    }
    case MDIncrementalRefreshTradeSummary48::TEMPLATE_ID: {
        MDIncrementalRefreshTradeSummary48 msg;
        msg.decode(data, offset);
        std::cout << "  [Trade48] time=" << formatTimestamp(msg.transactTime)
                  << " trades=" << msg.mdEntries.size() << "\n";
        for (const auto& e : msg.mdEntries) {
            std::cout << "    Trade SecID=" << e.securityID
                      << " Price=" << Price{e.mdEntryPx}.toDouble()
                      << " Size=" << e.mdEntrySize
                      << " Aggressor=" << aggressorSideStr(e.aggressorSide)
                      << " RptSeq=" << e.rptSeq << "\n";
        }
        break;
    }
    case SnapshotFullRefresh52::TEMPLATE_ID: {
        SnapshotFullRefresh52 msg;
        msg.decode(data, offset);
        std::cout << "  [Snapshot52] SecID=" << msg.securityID
                  << " RptSeq=" << msg.rptSeq
                  << " Status=" << tradingStatusStr(msg.mdSecurityTradingStatus)
                  << " Levels=" << msg.entries.size() << "\n";
        for (const auto& e : msg.entries) {
            std::cout << "    " << entryTypeStr(e.mdEntryType)
                      << " Level=" << static_cast<int>(e.mdPriceLevel)
                      << " Price=" << Price{e.mdEntryPx}.toDouble()
                      << " Size=" << e.mdEntrySize
                      << " Orders=" << e.numberOfOrders << "\n";
        }
        break;
    }
    case MDInstrumentDefinitionFuture54::TEMPLATE_ID: {
        MDInstrumentDefinitionFuture54 msg;
        msg.decode(data, offset);
        char sym[21], grp[7], asset[7];
        readFixedString(sym, msg.symbol, 20);
        readFixedString(grp, msg.securityGroup, 6);
        readFixedString(asset, msg.asset, 6);
        std::cout << "  [InstrDef54] Symbol=" << sym
                  << " SecID=" << msg.securityID
                  << " Group=" << grp
                  << " Asset=" << asset
                  << " Status=" << tradingStatusStr(msg.mdSecurityTradingStatus)
                  << " TickSize=" << Price{msg.minPriceIncrement}.toDouble()
                  << "\n";
        break;
    }
    case SecurityStatus30::TEMPLATE_ID: {
        SecurityStatus30 msg;
        msg.decode(data, offset);
        char grp[7], asset[7];
        readFixedString(grp, msg.securityGroup, 6);
        readFixedString(asset, msg.asset, 6);
        std::cout << "  [SecStatus30] SecID=" << msg.securityID
                  << " Group=" << grp
                  << " Status=" << tradingStatusStr(msg.securityTradingStatus)
                  << " Event=" << static_cast<int>(msg.securityTradingEvent)
                  << "\n";
        break;
    }
    case AdminHeartbeat12::TEMPLATE_ID: {
        std::cout << "  [Heartbeat12]\n";
        break;
    }
    case ChannelReset4::TEMPLATE_ID: {
        ChannelReset4 msg;
        msg.decode(data, offset);
        std::cout << "  [ChannelReset4] entries=" << msg.entries.size();
        for (const auto& e : msg.entries) {
            std::cout << " ApplID=" << e.applID;
        }
        std::cout << "\n";
        break;
    }
    default:
        std::cout << "  [Unknown templateId=" << templateId
                  << " blockLength=" << blockLength << "]\n";
        break;
    }
}

// ---------------------------------------------------------------------------
// Process a complete UDP packet
// ---------------------------------------------------------------------------
static void processPacket(const char* data, size_t len, Stats& stats) {
    if (len < PacketHeader::SIZE) {
        std::cerr << "Packet too short: " << len << " bytes\n";
        return;
    }

    uint32_t seqNum = PacketHeader::decodeMsgSeqNum(data);
    uint64_t sendingTime = PacketHeader::decodeSendingTime(data);

    // Check for sequence gaps
    if (stats.lastSeqNum > 0 && seqNum != stats.lastSeqNum + 1) {
        uint32_t gap = seqNum - stats.lastSeqNum - 1;
        std::cout << "*** GAP detected: expected " << (stats.lastSeqNum + 1)
                  << " got " << seqNum << " (missing " << gap << " packets)\n";
        stats.gaps += gap;
    }
    stats.lastSeqNum = seqNum;
    stats.packetsReceived++;

    std::cout << "[Pkt#" << seqNum << "] time="
              << formatTimestamp(sendingTime) << "\n";

    // Decode SBE messages after the packet header
    size_t offset = PacketHeader::SIZE;
    while (offset + MessageHeader::SIZE <= len) {
        uint16_t blockLength = MessageHeader::decodeBlockLength(data + offset);
        uint16_t templateId = MessageHeader::decodeTemplateId(data + offset);

        decodeMessage(data, offset, len, stats);

        // Advance past this message.
        // For messages with repeating groups, we need to compute the actual size.
        // Use a conservative approach: decode and use encodedLength.
        size_t msgSize = 0;
        switch (templateId) {
        case MDIncrementalRefreshBook46::TEMPLATE_ID: {
            MDIncrementalRefreshBook46 msg;
            msg.decode(data, offset);
            msgSize = msg.encodedLength();
            break;
        }
        case MDIncrementalRefreshTradeSummary48::TEMPLATE_ID: {
            MDIncrementalRefreshTradeSummary48 msg;
            msg.decode(data, offset);
            msgSize = msg.encodedLength();
            break;
        }
        case SnapshotFullRefresh52::TEMPLATE_ID: {
            SnapshotFullRefresh52 msg;
            msg.decode(data, offset);
            msgSize = msg.encodedLength();
            break;
        }
        case ChannelReset4::TEMPLATE_ID: {
            ChannelReset4 msg;
            msg.decode(data, offset);
            msgSize = msg.encodedLength();
            break;
        }
        default:
            // Fixed-size messages
            msgSize = MessageHeader::SIZE + blockLength;
            break;
        }

        if (msgSize == 0) break;
        offset += msgSize;
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    int channelId = 310;
    FeedType feedType = FeedType::Incremental;
    std::string overrideGroup;
    uint16_t overridePort = 0;
    std::string listenIface = "0.0.0.0";
    int statsInterval = 10;  // seconds

    // Parse args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--channel" && i + 1 < argc) {
            channelId = std::stoi(argv[++i]);
        } else if (arg == "--feed" && i + 1 < argc) {
            std::string f = argv[++i];
            if (f == "incremental") feedType = FeedType::Incremental;
            else if (f == "snapshot") feedType = FeedType::Snapshot;
            else if (f == "instdef") feedType = FeedType::InstrumentDef;
            else {
                std::cerr << "Unknown feed type: " << f << "\n";
                return 1;
            }
        } else if (arg == "--group" && i + 1 < argc) {
            overrideGroup = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            overridePort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--iface" && i + 1 < argc) {
            listenIface = argv[++i];
        } else if (arg == "--stats" && i + 1 < argc) {
            statsInterval = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: mdp3_listener [OPTIONS]\n"
                      << "  --channel ID      Channel ID (default: 310)\n"
                      << "  --feed TYPE       Feed type: incremental|snapshot|instdef"
                      << " (default: incremental)\n"
                      << "  --group ADDR      Override multicast group address\n"
                      << "  --port PORT       Override port\n"
                      << "  --iface ADDR      Listen interface (default: 0.0.0.0)\n"
                      << "  --stats N         Print stats every N seconds"
                      << " (default: 10)\n"
                      << "  --help            Show this help\n";
            return 0;
        }
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Resolve multicast group and port
    ChannelConfig cfg = getDefaultChannel(channelId);
    std::string multicastGroup;
    uint16_t port;

    switch (feedType) {
        case FeedType::Incremental:
            multicastGroup = overrideGroup.empty() ? cfg.incrementalAddr : overrideGroup;
            port = overridePort ? overridePort : cfg.incrementalPort;
            break;
        case FeedType::Snapshot:
            multicastGroup = overrideGroup.empty() ? cfg.snapshotAddr : overrideGroup;
            port = overridePort ? overridePort : cfg.snapshotPort;
            break;
        case FeedType::InstrumentDef:
            multicastGroup = overrideGroup.empty() ? cfg.instdefAddr : overrideGroup;
            port = overridePort ? overridePort : cfg.instdefPort;
            break;
    }

    const char* feedName = (feedType == FeedType::Incremental) ? "incremental"
                         : (feedType == FeedType::Snapshot) ? "snapshot"
                         : "instdef";

    std::cout << "MDP 3.0 Listener\n"
              << "  Channel:  " << channelId << "\n"
              << "  Feed:     " << feedName << "\n"
              << "  Group:    " << multicastGroup << "\n"
              << "  Port:     " << port << "\n"
              << "  Interface:" << listenIface << "\n"
              << std::endl;

    try {
        asio::io_context ioc;

        // Create UDP socket
        udp::endpoint listenEndpoint(
            asio::ip::make_address(listenIface), port);
        udp::socket sock(ioc, listenEndpoint.protocol());

        // Allow multiple listeners on same port
        sock.set_option(udp::socket::reuse_address(true));
        sock.bind(listenEndpoint);

        // Join multicast group
        sock.set_option(asio::ip::multicast::join_group(
            asio::ip::make_address(multicastGroup)));

        std::cout << "Joined multicast group " << multicastGroup
                  << ":" << port << "\n"
                  << "Listening for packets... (Ctrl+C to stop)\n\n";

        Stats stats;
        stats.startTime = std::chrono::steady_clock::now();
        stats.lastPrintTime = stats.startTime;

        char recvBuf[65536];
        udp::endpoint senderEndpoint;

        while (g_running) {
            // Use a short timeout so we can check g_running periodically
            sock.non_blocking(true);

            boost::system::error_code ec;
            size_t bytesRead = sock.receive_from(
                asio::buffer(recvBuf, sizeof(recvBuf)),
                senderEndpoint, 0, ec);

            if (ec == asio::error::would_block) {
                // No data available, sleep briefly
                std::this_thread::sleep_for(std::chrono::milliseconds(1));

                // Periodic stats
                auto now = std::chrono::steady_clock::now();
                auto sinceLastPrint =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        now - stats.lastPrintTime)
                        .count();
                if (sinceLastPrint >= statsInterval && stats.packetsReceived > 0) {
                    printStats(stats);
                }
                continue;
            }
            if (ec) {
                if (g_running) {
                    std::cerr << "Receive error: " << ec.message() << "\n";
                }
                break;
            }

            processPacket(recvBuf, bytesRead, stats);

            // Periodic stats
            auto now = std::chrono::steady_clock::now();
            auto sinceLastPrint =
                std::chrono::duration_cast<std::chrono::seconds>(
                    now - stats.lastPrintTime)
                    .count();
            if (sinceLastPrint >= statsInterval) {
                printStats(stats);
            }
        }

        // Final stats
        std::cout << "\n--- Final Statistics ---\n";
        printStats(stats);

        sock.close();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
