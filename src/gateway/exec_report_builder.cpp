#include "exec_report_builder.h"
#include "../sbe/ilink3_messages.h"
#include <chrono>
#include <cstring>

namespace cme::sim::gateway {

// Convert OrdStatus enum (numeric values) to FIX char representation
static char ordStatusToChar(OrdStatus status) {
    switch (status) {
        case OrdStatus::New:             return '0';
        case OrdStatus::PartiallyFilled: return '1';
        case OrdStatus::Filled:          return '2';
        case OrdStatus::Canceled:        return '4';
        case OrdStatus::Replaced:        return '5';
        case OrdStatus::Rejected:        return '8';
        default:                         return '0';
    }
}

static uint64_t currentTimeNanos() {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count());
}

std::string ExecReportBuilder::generateExecId() {
    return std::to_string(next_exec_id_++);
}

std::vector<char> ExecReportBuilder::buildExecutionReportNew(
    const OrderAccepted& event, uint64_t uuid) {

    sbe::ExecutionReportNew522 msg;
    msg.uuid = uuid;
    msg.orderID = event.order_id;
    msg.price = event.price.mantissa;
    msg.securityID = event.security_id;
    msg.orderQty = static_cast<uint32_t>(event.quantity);
    msg.side = static_cast<uint8_t>(event.side);
    msg.ordType = static_cast<uint8_t>(event.order_type);
    msg.timeInForce = static_cast<uint8_t>(event.time_in_force);

    auto now = currentTimeNanos();
    msg.transactTime = now;
    msg.sendingTimeEpoch = now;

    std::string exec_id = generateExecId();
    sbe::writeFixedString(msg.execID, exec_id.c_str(), 40);
    sbe::writeFixedString(msg.clOrdID, event.cl_ord_id.c_str(), 20);

    // Encode SBE header + body only (TcpConnection adds SOFH framing)
    size_t sbe_len = msg.encodedLength();
    std::vector<char> buf(sbe_len, 0);
    msg.encode(buf.data(), 0);

    return buf;
}

std::vector<char> ExecReportBuilder::buildExecutionReportReject(
    const OrderRejected& event, uint64_t uuid) {

    sbe::ExecutionReportReject523 msg;
    msg.uuid = uuid;
    msg.ordRejReason = event.reject_reason_code;

    auto now = currentTimeNanos();
    msg.transactTime = now;
    msg.sendingTimeEpoch = now;

    std::string exec_id = generateExecId();
    sbe::writeFixedString(msg.execID, exec_id.c_str(), 40);
    sbe::writeFixedString(msg.clOrdID, event.cl_ord_id.c_str(), 20);

    msg.ordStatus = '8';
    msg.execType = '8';

    size_t sbe_len = msg.encodedLength();
    std::vector<char> buf(sbe_len, 0);
    msg.encode(buf.data(), 0);

    return buf;
}

std::vector<char> ExecReportBuilder::buildExecutionReportFill(
    const OrderFilled& event, uint64_t uuid, bool is_maker) {

    sbe::ExecutionReportTradeOutright525 msg;
    msg.uuid = uuid;

    if (is_maker) {
        msg.orderID = event.maker_order_id;
        sbe::writeFixedString(msg.clOrdID, event.maker_cl_ord_id.c_str(), 20);
        msg.cumQty = static_cast<uint32_t>(event.maker_cum_qty);
        msg.leavesQty = static_cast<uint32_t>(event.maker_leaves_qty);
        msg.side = (event.aggressor_side == Side::Buy)
            ? static_cast<uint8_t>(Side::Sell) : static_cast<uint8_t>(Side::Buy);
        msg.aggressorIndicator = 0; // passive
        msg.ordStatus = ordStatusToChar(event.maker_ord_status);
    } else {
        msg.orderID = event.taker_order_id;
        sbe::writeFixedString(msg.clOrdID, event.taker_cl_ord_id.c_str(), 20);
        msg.cumQty = static_cast<uint32_t>(event.taker_cum_qty);
        msg.leavesQty = static_cast<uint32_t>(event.taker_leaves_qty);
        msg.side = static_cast<uint8_t>(event.aggressor_side);
        msg.aggressorIndicator = 1; // aggressor
        msg.ordStatus = ordStatusToChar(event.taker_ord_status);
    }

    msg.securityID = event.security_id;
    msg.price = event.trade_price.mantissa;
    msg.lastPx = event.trade_price.mantissa;
    msg.lastQty = static_cast<uint32_t>(event.trade_qty);
    msg.fillPx = event.trade_price.mantissa;
    msg.fillQty = static_cast<uint32_t>(event.trade_qty);
    msg.execType = 'F';

    auto now = currentTimeNanos();
    msg.transactTime = now;
    msg.sendingTimeEpoch = now;

    std::string exec_id = generateExecId();
    sbe::writeFixedString(msg.execID, exec_id.c_str(), 40);

    size_t sbe_len = msg.encodedLength();
    std::vector<char> buf(sbe_len, 0);
    msg.encode(buf.data(), 0);

    return buf;
}

std::vector<char> ExecReportBuilder::buildExecutionReportCancel(
    const OrderCancelled& event, uint64_t uuid) {

    sbe::ExecutionReportCancel534 msg;
    msg.uuid = uuid;
    msg.orderID = event.order_id;
    msg.securityID = event.security_id;
    msg.cumQty = static_cast<uint32_t>(event.cum_qty);
    msg.ordStatus = '4';
    msg.execType = '4';

    auto now = currentTimeNanos();
    msg.transactTime = now;
    msg.sendingTimeEpoch = now;

    std::string exec_id = generateExecId();
    sbe::writeFixedString(msg.execID, exec_id.c_str(), 40);
    sbe::writeFixedString(msg.clOrdID, event.cl_ord_id.c_str(), 20);

    size_t sbe_len = msg.encodedLength();
    std::vector<char> buf(sbe_len, 0);
    msg.encode(buf.data(), 0);

    return buf;
}

std::vector<char> ExecReportBuilder::buildExecutionReportModify(
    const OrderModified& event, uint64_t uuid) {

    sbe::ExecutionReportModify531 msg;
    msg.uuid = uuid;
    msg.orderID = event.order_id;
    msg.securityID = event.security_id;
    msg.price = event.new_price.mantissa;
    msg.orderQty = static_cast<uint32_t>(event.new_qty);
    msg.cumQty = static_cast<uint32_t>(event.cum_qty);
    msg.ordStatus = '0'; // New (replaced)
    msg.execType = '5'; // Replace

    auto now = currentTimeNanos();
    msg.transactTime = now;
    msg.sendingTimeEpoch = now;

    std::string exec_id = generateExecId();
    sbe::writeFixedString(msg.execID, exec_id.c_str(), 40);
    sbe::writeFixedString(msg.clOrdID, event.cl_ord_id.c_str(), 20);

    size_t sbe_len = msg.encodedLength();
    std::vector<char> buf(sbe_len, 0);
    msg.encode(buf.data(), 0);

    return buf;
}

std::vector<char> ExecReportBuilder::buildExecutionReportElimination(
    const OrderAccepted& event, uint64_t uuid) {

    sbe::ExecutionReportElimination524 msg;
    msg.uuid = uuid;
    msg.orderID = event.order_id;
    msg.securityID = event.security_id;
    msg.price = event.price.mantissa;
    msg.orderQty = static_cast<uint32_t>(event.quantity);
    msg.side = static_cast<uint8_t>(event.side);
    msg.ordType = static_cast<uint8_t>(event.order_type);
    msg.timeInForce = static_cast<uint8_t>(event.time_in_force);
    msg.cumQty = 0;
    msg.ordStatus = 'C'; // Expired/Eliminated
    msg.execType = 'C';

    auto now = currentTimeNanos();
    msg.transactTime = now;
    msg.sendingTimeEpoch = now;

    std::string exec_id = generateExecId();
    sbe::writeFixedString(msg.execID, exec_id.c_str(), 40);
    sbe::writeFixedString(msg.clOrdID, event.cl_ord_id.c_str(), 20);

    size_t sbe_len = msg.encodedLength();
    std::vector<char> buf(sbe_len, 0);
    msg.encode(buf.data(), 0);

    return buf;
}

std::vector<char> ExecReportBuilder::buildOrderCancelReject(
    const OrderCancelRejected& event, uint64_t uuid) {

    sbe::OrderCancelReject535 msg;
    msg.uuid = uuid;
    msg.orderID = event.order_id;
    msg.cxlRejReason = event.reject_reason_code;

    auto now = currentTimeNanos();
    msg.transactTime = now;
    msg.sendingTimeEpoch = now;

    std::string exec_id = generateExecId();
    sbe::writeFixedString(msg.execID, exec_id.c_str(), 40);
    sbe::writeFixedString(msg.clOrdID, event.cl_ord_id.c_str(), 20);

    size_t sbe_len = msg.encodedLength();
    std::vector<char> buf(sbe_len, 0);
    msg.encode(buf.data(), 0);

    return buf;
}

std::vector<char> ExecReportBuilder::buildFromEvent(
    const EngineEvent& event, uint64_t session_uuid) {

    return std::visit([this, session_uuid](const auto& e) -> std::vector<char> {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, OrderAccepted>) {
            return buildExecutionReportNew(e, session_uuid);
        } else if constexpr (std::is_same_v<T, OrderRejected>) {
            return buildExecutionReportReject(e, session_uuid);
        } else if constexpr (std::is_same_v<T, OrderFilled>) {
            // This path builds for the session_uuid side
            bool is_maker = (e.maker_session_uuid == session_uuid);
            return buildExecutionReportFill(e, session_uuid, is_maker);
        } else if constexpr (std::is_same_v<T, OrderCancelled>) {
            return buildExecutionReportCancel(e, session_uuid);
        } else if constexpr (std::is_same_v<T, OrderModified>) {
            return buildExecutionReportModify(e, session_uuid);
        } else if constexpr (std::is_same_v<T, OrderCancelRejected>) {
            return buildOrderCancelReject(e, session_uuid);
        } else {
            // BookUpdate - not sent to sessions as exec reports
            return {};
        }
    }, event);
}

} // namespace cme::sim::gateway
