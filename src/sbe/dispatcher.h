#pragma once
#include <cstdint>
#include <cstddef>
#include "message_header.h"
#include "ilink3_messages.h"
#include "mdp3_messages.h"

namespace cme::sim::sbe {

// Dispatches an iLink 3 SBE message to a typed handler.
// The buffer should point to the start of the SBE message header (after SOFH).
// Handler must have operator() overloads for each message type it handles.
template<typename Handler>
bool dispatchILink3Message(const char* buffer, size_t length, Handler& handler) {
    if (length < MessageHeader::SIZE) return false;

    uint16_t templateId = MessageHeader::decodeTemplateId(buffer);

    switch (templateId) {
    case Negotiate500::TEMPLATE_ID: {
        Negotiate500 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case NegotiationResponse501::TEMPLATE_ID: {
        NegotiationResponse501 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case Establish503::TEMPLATE_ID: {
        Establish503 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case EstablishmentAck504::TEMPLATE_ID: {
        EstablishmentAck504 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case Sequence506::TEMPLATE_ID: {
        Sequence506 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case Terminate507::TEMPLATE_ID: {
        Terminate507 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case RetransmitRequest508::TEMPLATE_ID: {
        RetransmitRequest508 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case Retransmission509::TEMPLATE_ID: {
        Retransmission509 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case NotApplied513::TEMPLATE_ID: {
        NotApplied513 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case NewOrderSingle514::TEMPLATE_ID: {
        NewOrderSingle514 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case OrderCancelReplaceRequest515::TEMPLATE_ID: {
        OrderCancelReplaceRequest515 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case OrderCancelRequest516::TEMPLATE_ID: {
        OrderCancelRequest516 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case ExecutionReportNew522::TEMPLATE_ID: {
        ExecutionReportNew522 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case ExecutionReportReject523::TEMPLATE_ID: {
        ExecutionReportReject523 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case ExecutionReportElimination524::TEMPLATE_ID: {
        ExecutionReportElimination524 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case ExecutionReportTradeOutright525::TEMPLATE_ID: {
        ExecutionReportTradeOutright525 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case ExecutionReportModify531::TEMPLATE_ID: {
        ExecutionReportModify531 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case ExecutionReportCancel534::TEMPLATE_ID: {
        ExecutionReportCancel534 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case OrderCancelReject535::TEMPLATE_ID: {
        OrderCancelReject535 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    default:
        return false;
    }
}

// Dispatches an MDP 3.0 SBE message to a typed handler.
// The buffer should point to the start of the SBE message header
// (after the MDP packet header).
// Handler must have operator() overloads for each message type it handles.
template<typename Handler>
bool dispatchMDP3Message(const char* buffer, size_t length, Handler& handler) {
    if (length < MessageHeader::SIZE) return false;

    uint16_t templateId = MessageHeader::decodeTemplateId(buffer);

    switch (templateId) {
    case ChannelReset4::TEMPLATE_ID: {
        ChannelReset4 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case AdminHeartbeat12::TEMPLATE_ID: {
        AdminHeartbeat12 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case SecurityStatus30::TEMPLATE_ID: {
        SecurityStatus30 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case MDIncrementalRefreshBook46::TEMPLATE_ID: {
        MDIncrementalRefreshBook46 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case MDIncrementalRefreshTradeSummary48::TEMPLATE_ID: {
        MDIncrementalRefreshTradeSummary48 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case SnapshotFullRefresh52::TEMPLATE_ID: {
        SnapshotFullRefresh52 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    case MDInstrumentDefinitionFuture54::TEMPLATE_ID: {
        MDInstrumentDefinitionFuture54 msg;
        msg.decode(buffer, 0);
        handler(msg);
        return true;
    }
    default:
        return false;
    }
}

} // namespace cme::sim::sbe
