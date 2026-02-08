// ilink3_client - Test client for iLink 3 order entry
// Usage: ilink3_client [--host HOST] [--port PORT] [--auto N] [--interactive]

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
#include <sstream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <map>
#include <cstring>
#include <csignal>

#include "sbe/ilink3_messages.h"
#include "sbe/framing.h"
#include "sbe/message_header.h"
#include "common/types.h"

using boost::asio::ip::tcp;
namespace asio = boost::asio;
using namespace cme::sim::sbe;
using namespace cme::sim;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static std::atomic<bool> g_running{true};
static std::mutex g_io_mutex;
static std::mutex g_send_mutex;

static void signalHandler(int) { g_running = false; }

// Thread-safe print
template <typename... Args>
static void tprint(Args&&... args) {
    std::lock_guard<std::mutex> lk(g_io_mutex);
    (std::cout << ... << std::forward<Args>(args));
    std::cout << std::flush;
}

// Current time in nanoseconds since epoch
static uint64_t nowNanos() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

// ---------------------------------------------------------------------------
// Session state for the client
// ---------------------------------------------------------------------------
struct ClientSession {
    uint64_t uuid = 0;
    uint32_t nextOutSeqNo = 1;
    uint32_t nextInSeqNo = 1;
    uint16_t keepAliveIntervalMs = 30000;
    std::string state = "Disconnected";
    uint64_t nextOrderRequestID = 1;
    uint32_t nextClOrdCounter = 1;

    // Track order IDs for cancel/modify
    std::map<uint64_t, std::string> orderMap; // orderID -> description

    // Statistics
    uint32_t ordersSent = 0;
    uint32_t ordersAccepted = 0;
    uint32_t ordersRejected = 0;
    uint32_t ordersCancelled = 0;
    uint32_t ordersModified = 0;
    uint32_t fills = 0;
};

// ---------------------------------------------------------------------------
// Send a SOFH-framed SBE message over TCP
// ---------------------------------------------------------------------------
static void sendFramed(tcp::socket& sock, const char* sbe_data, size_t sbe_len) {
    char sofh[SOFH::SIZE];
    SOFH::encode(sofh, SOFH::framedLength(static_cast<uint32_t>(sbe_len)));

    std::lock_guard<std::mutex> lk(g_send_mutex);
    boost::system::error_code ec;
    asio::write(sock, asio::buffer(sofh, SOFH::SIZE), ec);
    if (!ec) {
        asio::write(sock, asio::buffer(sbe_data, sbe_len), ec);
    }
    if (ec) {
        tprint("Send error: ", ec.message(), "\n");
    }
}

// ---------------------------------------------------------------------------
// Negotiate -> Establish handshake
// ---------------------------------------------------------------------------
static bool performHandshake(tcp::socket& sock, ClientSession& session) {
    char buf[4096];

    // Generate UUID
    session.uuid = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());

    // -- Send Negotiate500 --
    Negotiate500 neg;
    neg.uuid = session.uuid;
    neg.sendingTime = nowNanos();
    writeFixedString(neg.accessKeyID, "TestClient", 20);
    writeFixedString(neg.session, "T01", 3);
    writeFixedString(neg.firm, "TEST", 5);
    neg.cancelOnDisconnectIndicator = 1;

    size_t sbeLen = neg.encode(buf, 0);
    sendFramed(sock, buf, sbeLen);
    session.state = "NegotiateSent";
    tprint("[CLIENT] Sent Negotiate500 (UUID=", session.uuid, ")\n");

    // -- Read NegotiationResponse501 --
    char sofhBuf[SOFH::SIZE];
    boost::system::error_code ec;
    asio::read(sock, asio::buffer(sofhBuf, SOFH::SIZE), ec);
    if (ec) {
        tprint("[CLIENT] Failed to read SOFH: ", ec.message(), "\n");
        return false;
    }

    uint32_t msgLen = SOFH::decodeMessageLength(sofhBuf);
    uint32_t payloadLen = msgLen - static_cast<uint32_t>(SOFH::SIZE);
    if (payloadLen > sizeof(buf)) {
        tprint("[CLIENT] Message too large: ", payloadLen, "\n");
        return false;
    }

    asio::read(sock, asio::buffer(buf, payloadLen), ec);
    if (ec) {
        tprint("[CLIENT] Failed to read payload: ", ec.message(), "\n");
        return false;
    }

    uint16_t templateId = MessageHeader::decodeTemplateId(buf);
    if (templateId != NegotiationResponse501::TEMPLATE_ID) {
        tprint("[CLIENT] Expected NegotiationResponse501, got templateId=",
               templateId, "\n");
        return false;
    }

    NegotiationResponse501 negResp;
    negResp.decode(buf, 0);
    tprint("[CLIENT] Received NegotiationResponse501 (UUID=", negResp.uuid,
           ", PreviousSeqNo=", negResp.previousSeqNo, ")\n");
    session.state = "Negotiated";

    // -- Send Establish503 --
    Establish503 est;
    est.uuid = session.uuid;
    est.sendingTime = nowNanos();
    writeFixedString(est.accessKeyID, "TestClient", 20);
    writeFixedString(est.session, "T01", 3);
    writeFixedString(est.firm, "TEST", 5);
    est.keepAliveInterval = 30000;  // 30 seconds (in milliseconds)
    est.nextSeqNo = session.nextOutSeqNo;

    sbeLen = est.encode(buf, 0);
    sendFramed(sock, buf, sbeLen);
    session.state = "EstablishSent";
    tprint("[CLIENT] Sent Establish503\n");

    // -- Read EstablishmentAck504 --
    asio::read(sock, asio::buffer(sofhBuf, SOFH::SIZE), ec);
    if (ec) {
        tprint("[CLIENT] Failed to read SOFH: ", ec.message(), "\n");
        return false;
    }

    msgLen = SOFH::decodeMessageLength(sofhBuf);
    payloadLen = msgLen - static_cast<uint32_t>(SOFH::SIZE);
    if (payloadLen > sizeof(buf)) {
        tprint("[CLIENT] Message too large: ", payloadLen, "\n");
        return false;
    }

    asio::read(sock, asio::buffer(buf, payloadLen), ec);
    if (ec) {
        tprint("[CLIENT] Failed to read payload: ", ec.message(), "\n");
        return false;
    }

    templateId = MessageHeader::decodeTemplateId(buf);
    if (templateId != EstablishmentAck504::TEMPLATE_ID) {
        tprint("[CLIENT] Expected EstablishmentAck504, got templateId=",
               templateId, "\n");
        return false;
    }

    EstablishmentAck504 estAck;
    estAck.decode(buf, 0);
    session.keepAliveIntervalMs = estAck.keepAliveInterval;
    tprint("[CLIENT] Received EstablishmentAck504 (KeepAlive=",
           estAck.keepAliveInterval, "ms, NextSeqNo=", estAck.nextSeqNo, ")\n");
    session.nextInSeqNo = estAck.nextSeqNo;
    session.state = "Established";
    tprint("[CLIENT] Session ESTABLISHED\n");

    return true;
}

// ---------------------------------------------------------------------------
// Send NewOrderSingle514
// ---------------------------------------------------------------------------
static void sendOrder(tcp::socket& sock, ClientSession& session,
                      uint8_t side, int32_t securityID,
                      uint32_t qty, double price) {
    char buf[4096];
    NewOrderSingle514 nos;
    nos.price = Price::fromDouble(price).mantissa;
    nos.orderQty = qty;
    nos.securityID = securityID;
    nos.side = side;
    nos.seqNum = session.nextOutSeqNo++;
    nos.orderRequestID = session.nextOrderRequestID++;
    nos.sendingTimeEpoch = nowNanos();
    nos.ordType = static_cast<uint8_t>(OrderType::Limit);
    nos.timeInForce = static_cast<uint8_t>(TimeInForce::Day);
    nos.manualOrderIndicator = 0;
    nos.displayQty = qty;

    std::string clOrdStr = "ORD-" + std::to_string(session.nextClOrdCounter++);
    writeFixedString(nos.clOrdID, clOrdStr.c_str(), 20);
    writeFixedString(nos.senderID, "TestClient", 20);
    writeFixedString(nos.location, "US,NY", 5);

    size_t sbeLen = nos.encode(buf, 0);
    sendFramed(sock, buf, sbeLen);
    session.ordersSent++;

    const char* sideStr = (side == 1) ? "Buy" : "Sell";
    tprint("[CLIENT] Sent NewOrderSingle514: ", sideStr,
           " SecID=", securityID,
           " Qty=", qty,
           " Price=", price,
           " ClOrdID=", clOrdStr, "\n");
}

// ---------------------------------------------------------------------------
// Send OrderCancelRequest516
// ---------------------------------------------------------------------------
static void sendCancel(tcp::socket& sock, ClientSession& session,
                       uint64_t orderID) {
    char buf[4096];
    OrderCancelRequest516 ocr;
    ocr.orderID = orderID;
    ocr.seqNum = session.nextOutSeqNo++;
    ocr.sendingTimeEpoch = nowNanos();
    ocr.orderRequestID = session.nextOrderRequestID++;
    ocr.side = static_cast<uint8_t>(Side::Buy);
    ocr.manualOrderIndicator = 0;

    std::string clOrdStr = "CXL-" + std::to_string(session.nextClOrdCounter++);
    writeFixedString(ocr.clOrdID, clOrdStr.c_str(), 20);
    writeFixedString(ocr.senderID, "TestClient", 20);
    writeFixedString(ocr.location, "US,NY", 5);

    size_t sbeLen = ocr.encode(buf, 0);
    sendFramed(sock, buf, sbeLen);

    tprint("[CLIENT] Sent OrderCancelRequest516: OrderID=", orderID, "\n");
}

// ---------------------------------------------------------------------------
// Send OrderCancelReplaceRequest515
// ---------------------------------------------------------------------------
static void sendModify(tcp::socket& sock, ClientSession& session,
                       uint64_t orderID, uint32_t newQty, double newPrice) {
    char buf[4096];
    OrderCancelReplaceRequest515 ocrr;
    ocrr.orderID = orderID;
    ocrr.price = Price::fromDouble(newPrice).mantissa;
    ocrr.orderQty = newQty;
    ocrr.seqNum = session.nextOutSeqNo++;
    ocrr.sendingTimeEpoch = nowNanos();
    ocrr.orderRequestID = session.nextOrderRequestID++;
    ocrr.side = static_cast<uint8_t>(Side::Buy);
    ocrr.ordType = static_cast<uint8_t>(OrderType::Limit);
    ocrr.timeInForce = static_cast<uint8_t>(TimeInForce::Day);
    ocrr.manualOrderIndicator = 0;
    ocrr.displayQty = newQty;

    std::string clOrdStr = "MOD-" + std::to_string(session.nextClOrdCounter++);
    writeFixedString(ocrr.clOrdID, clOrdStr.c_str(), 20);
    writeFixedString(ocrr.senderID, "TestClient", 20);
    writeFixedString(ocrr.location, "US,NY", 5);

    size_t sbeLen = ocrr.encode(buf, 0);
    sendFramed(sock, buf, sbeLen);

    tprint("[CLIENT] Sent OrderCancelReplaceRequest515: OrderID=", orderID,
           " NewQty=", newQty, " NewPrice=", newPrice, "\n");
}

// ---------------------------------------------------------------------------
// Send Sequence506 heartbeat
// ---------------------------------------------------------------------------
static void sendSequenceHeartbeat(tcp::socket& sock, ClientSession& session) {
    char buf[256];
    Sequence506 seq;
    seq.uuid = session.uuid;
    seq.nextSeqNo = session.nextOutSeqNo;
    seq.faultToleranceIndicator = 0;
    seq.keepAliveIntervalLapsed = 0;

    size_t sbeLen = seq.encode(buf, 0);
    sendFramed(sock, buf, sbeLen);
}

// ---------------------------------------------------------------------------
// Send Terminate507
// ---------------------------------------------------------------------------
static void sendTerminate(tcp::socket& sock, ClientSession& session) {
    char buf[256];
    Terminate507 term;
    term.uuid = session.uuid;
    term.requestTimestamp = nowNanos();
    term.errorCodes = 0;

    size_t sbeLen = term.encode(buf, 0);
    sendFramed(sock, buf, sbeLen);
    session.state = "Terminated";
    tprint("[CLIENT] Sent Terminate507\n");
}

// ---------------------------------------------------------------------------
// Process a received SBE message
// ---------------------------------------------------------------------------
static void processMessage(const char* data, size_t len, ClientSession& session) {
    if (len < MessageHeader::SIZE) {
        tprint("[CLIENT] Message too short: ", len, " bytes\n");
        return;
    }

    uint16_t templateId = MessageHeader::decodeTemplateId(data);
    uint16_t blockLength = MessageHeader::decodeBlockLength(data);

    switch (templateId) {
    case ExecutionReportNew522::TEMPLATE_ID: {
        ExecutionReportNew522 er;
        er.decode(data, 0);
        char clOrdBuf[21];
        readFixedString(clOrdBuf, er.clOrdID, 20);
        session.orderMap[er.orderID] = clOrdBuf;
        session.ordersAccepted++;
        tprint("[CLIENT] << Order accepted: OrderID=", er.orderID,
               " ClOrdID=", clOrdBuf,
               " SecID=", er.securityID,
               " Side=", (er.side == 1 ? "Buy" : "Sell"),
               " Qty=", er.orderQty,
               " Price=", Price{er.price}.toDouble(), "\n");
        break;
    }
    case ExecutionReportReject523::TEMPLATE_ID: {
        ExecutionReportReject523 er;
        er.decode(data, 0);
        char clOrdBuf[21];
        readFixedString(clOrdBuf, er.clOrdID, 20);
        session.ordersRejected++;
        tprint("[CLIENT] << Order rejected: ClOrdID=", clOrdBuf,
               " Reason=", er.ordRejReason, "\n");
        break;
    }
    case ExecutionReportTradeOutright525::TEMPLATE_ID: {
        ExecutionReportTradeOutright525 er;
        er.decode(data, 0);
        session.fills++;
        tprint("[CLIENT] << Fill: OrderID=", er.orderID,
               " LastQty=", er.lastQty,
               " @ LastPx=", Price{er.lastPx}.toDouble(),
               " CumQty=", er.cumQty,
               " LeavesQty=", er.leavesQty,
               " Aggressor=", (er.aggressorIndicator ? "Y" : "N"), "\n");
        break;
    }
    case ExecutionReportCancel534::TEMPLATE_ID: {
        ExecutionReportCancel534 er;
        er.decode(data, 0);
        session.ordersCancelled++;
        session.orderMap.erase(er.orderID);
        tprint("[CLIENT] << Order cancelled: OrderID=", er.orderID,
               " CumQty=", er.cumQty, "\n");
        break;
    }
    case ExecutionReportModify531::TEMPLATE_ID: {
        ExecutionReportModify531 er;
        er.decode(data, 0);
        session.ordersModified++;
        tprint("[CLIENT] << Order modified: OrderID=", er.orderID,
               " Qty=", er.orderQty,
               " Price=", Price{er.price}.toDouble(), "\n");
        break;
    }
    case ExecutionReportElimination524::TEMPLATE_ID: {
        ExecutionReportElimination524 er;
        er.decode(data, 0);
        tprint("[CLIENT] << Order eliminated: OrderID=", er.orderID,
               " CumQty=", er.cumQty, "\n");
        break;
    }
    case OrderCancelReject535::TEMPLATE_ID: {
        OrderCancelReject535 cr;
        cr.decode(data, 0);
        tprint("[CLIENT] << Cancel rejected: OrderID=", cr.orderID,
               " Reason=", cr.cxlRejReason, "\n");
        break;
    }
    case Sequence506::TEMPLATE_ID: {
        Sequence506 seq;
        seq.decode(data, 0);
        // Server heartbeat; we may respond if lapsed
        if (seq.keepAliveIntervalLapsed) {
            tprint("[CLIENT] << Server keepalive lapsed, server expects response\n");
        }
        break;
    }
    case NegotiationResponse501::TEMPLATE_ID:
    case EstablishmentAck504::TEMPLATE_ID:
        // Already handled during handshake
        break;
    case Terminate507::TEMPLATE_ID: {
        Terminate507 term;
        term.decode(data, 0);
        tprint("[CLIENT] << Server terminated session (errorCodes=",
               term.errorCodes, ")\n");
        session.state = "Terminated";
        g_running = false;
        break;
    }
    case NotApplied513::TEMPLATE_ID: {
        NotApplied513 na;
        na.decode(data, 0);
        tprint("[CLIENT] << NotApplied: FromSeqNo=", na.fromSeqNo,
               " MsgCount=", na.msgCount, "\n");
        break;
    }
    default:
        tprint("[CLIENT] << Unknown message templateId=", templateId,
               " blockLength=", blockLength, "\n");
        break;
    }
}

// ---------------------------------------------------------------------------
// Reader thread: reads SOFH-framed messages from the socket
// ---------------------------------------------------------------------------
static void readerThread(tcp::socket& sock, ClientSession& session) {
    char sofhBuf[SOFH::SIZE];
    std::vector<char> payload(65536);

    while (g_running && session.state != "Terminated") {
        boost::system::error_code ec;
        asio::read(sock, asio::buffer(sofhBuf, SOFH::SIZE), ec);
        if (ec) {
            if (g_running) {
                tprint("[CLIENT] Read error: ", ec.message(), "\n");
            }
            g_running = false;
            break;
        }

        uint32_t msgLen = SOFH::decodeMessageLength(sofhBuf);
        uint32_t payloadLen = msgLen - static_cast<uint32_t>(SOFH::SIZE);
        if (payloadLen > payload.size()) {
            payload.resize(payloadLen);
        }

        asio::read(sock, asio::buffer(payload.data(), payloadLen), ec);
        if (ec) {
            if (g_running) {
                tprint("[CLIENT] Read payload error: ", ec.message(), "\n");
            }
            g_running = false;
            break;
        }

        processMessage(payload.data(), payloadLen, session);
    }
}

// ---------------------------------------------------------------------------
// Keepalive thread: sends periodic Sequence506 heartbeats
// ---------------------------------------------------------------------------
static void keepaliveThread(tcp::socket& sock, ClientSession& session) {
    while (g_running && session.state == "Established") {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(session.keepAliveIntervalMs / 2));
        if (g_running && session.state == "Established") {
            sendSequenceHeartbeat(sock, session);
        }
    }
}

// ---------------------------------------------------------------------------
// Interactive command loop
// ---------------------------------------------------------------------------
static void interactiveLoop(tcp::socket& sock, ClientSession& session) {
    tprint("\n--- iLink 3 Test Client ---\n");
    tprint("Commands:\n");
    tprint("  buy SECURITY_ID QTY PRICE   - Send buy order\n");
    tprint("  sell SECURITY_ID QTY PRICE  - Send sell order\n");
    tprint("  cancel ORDER_ID             - Cancel an order\n");
    tprint("  modify ORDER_ID QTY PRICE   - Modify an order\n");
    tprint("  status                      - Show session state\n");
    tprint("  orders                      - Show tracked orders\n");
    tprint("  quit                        - Terminate session\n\n");

    while (g_running && session.state == "Established") {
        tprint("> ");
        std::string line;
        if (!std::getline(std::cin, line)) {
            break;
        }

        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd)) continue;

        if (cmd == "buy" || cmd == "sell") {
            int32_t secId;
            uint32_t qty;
            double price;
            if (!(iss >> secId >> qty >> price)) {
                tprint("Usage: ", cmd, " SECURITY_ID QTY PRICE\n");
                continue;
            }
            uint8_t side = (cmd == "buy")
                ? static_cast<uint8_t>(Side::Buy)
                : static_cast<uint8_t>(Side::Sell);
            sendOrder(sock, session, side, secId, qty, price);
        } else if (cmd == "cancel") {
            uint64_t orderId;
            if (!(iss >> orderId)) {
                tprint("Usage: cancel ORDER_ID\n");
                continue;
            }
            sendCancel(sock, session, orderId);
        } else if (cmd == "modify") {
            uint64_t orderId;
            uint32_t qty;
            double price;
            if (!(iss >> orderId >> qty >> price)) {
                tprint("Usage: modify ORDER_ID QTY PRICE\n");
                continue;
            }
            sendModify(sock, session, orderId, qty, price);
        } else if (cmd == "status") {
            tprint("Session state: ", session.state, "\n");
            tprint("UUID: ", session.uuid, "\n");
            tprint("NextOutSeqNo: ", session.nextOutSeqNo, "\n");
            tprint("Orders sent: ", session.ordersSent, "\n");
            tprint("Orders accepted: ", session.ordersAccepted, "\n");
            tprint("Orders rejected: ", session.ordersRejected, "\n");
            tprint("Orders cancelled: ", session.ordersCancelled, "\n");
            tprint("Orders modified: ", session.ordersModified, "\n");
            tprint("Fills: ", session.fills, "\n");
        } else if (cmd == "orders") {
            if (session.orderMap.empty()) {
                tprint("No tracked orders\n");
            } else {
                for (const auto& [id, desc] : session.orderMap) {
                    tprint("  OrderID=", id, " ClOrdID=", desc, "\n");
                }
            }
        } else if (cmd == "quit") {
            sendTerminate(sock, session);
            g_running = false;
            break;
        } else {
            tprint("Unknown command: ", cmd, "\n");
        }
    }
}

// ---------------------------------------------------------------------------
// Automated test mode
// ---------------------------------------------------------------------------
static void autoTest(tcp::socket& sock, ClientSession& session, int numOrders) {
    tprint("[AUTO] Sending ", numOrders, " orders...\n");

    for (int i = 0; i < numOrders && g_running; ++i) {
        // Alternate buy/sell
        uint8_t side = (i % 2 == 0)
            ? static_cast<uint8_t>(Side::Buy)
            : static_cast<uint8_t>(Side::Sell);
        double price = 5000.0 + (i % 10) * 0.25;
        sendOrder(sock, session, side, 1 /* ESH5 */, 1, price);

        // Small delay between orders
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Wait for responses
    tprint("[AUTO] All orders sent, waiting for responses...\n");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    tprint("\n[AUTO] === Results ===\n");
    tprint("[AUTO] Orders sent:      ", session.ordersSent, "\n");
    tprint("[AUTO] Orders accepted:  ", session.ordersAccepted, "\n");
    tprint("[AUTO] Orders rejected:  ", session.ordersRejected, "\n");
    tprint("[AUTO] Orders cancelled: ", session.ordersCancelled, "\n");
    tprint("[AUTO] Fills:            ", session.fills, "\n");

    sendTerminate(sock, session);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    std::string port = "19000";
    bool interactive = true;
    int autoOrders = 0;

    // Parse args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = argv[++i];
        } else if (arg == "--auto" && i + 1 < argc) {
            autoOrders = std::stoi(argv[++i]);
            interactive = false;
        } else if (arg == "--interactive") {
            interactive = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: ilink3_client [OPTIONS]\n"
                      << "  --host HOST       Exchange host (default: 127.0.0.1)\n"
                      << "  --port PORT       Exchange port (default: 19000)\n"
                      << "  --auto N          Automated mode: send N orders\n"
                      << "  --interactive     Interactive mode (default)\n"
                      << "  --help            Show this help\n";
            return 0;
        }
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        asio::io_context ioc;
        tcp::resolver resolver(ioc);
        tcp::socket sock(ioc);

        tprint("[CLIENT] Connecting to ", host, ":", port, "...\n");
        auto endpoints = resolver.resolve(host, port);
        asio::connect(sock, endpoints);
        tprint("[CLIENT] Connected\n");

        ClientSession session;

        // Perform FIXP handshake
        if (!performHandshake(sock, session)) {
            tprint("[CLIENT] Handshake failed\n");
            return 1;
        }

        // Start reader thread
        std::thread reader(readerThread, std::ref(sock), std::ref(session));

        // Start keepalive thread
        std::thread keepalive(keepaliveThread, std::ref(sock), std::ref(session));

        if (interactive) {
            interactiveLoop(sock, session);
        } else {
            autoTest(sock, session, autoOrders);
        }

        g_running = false;

        // Shutdown socket to unblock reader
        boost::system::error_code ec;
        sock.shutdown(tcp::socket::shutdown_both, ec);
        sock.close(ec);

        if (reader.joinable()) reader.join();
        if (keepalive.joinable()) keepalive.join();

        tprint("[CLIENT] Disconnected\n");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
