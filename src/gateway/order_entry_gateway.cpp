#include "order_entry_gateway.h"
#include "../sbe/ilink3_messages.h"
#include "../sbe/framing.h"
#include "../sbe/message_header.h"
#include <cstring>

namespace cme::sim::gateway {

OrderEntryGateway::OrderEntryGateway(InstrumentManager& instrument_mgr,
                                     const config::RiskConfig& risk_config)
    : instrument_mgr_(instrument_mgr)
    , validator_(instrument_mgr)
    , risk_manager_(risk_config) {}

void OrderEntryGateway::onApplicationMessage(uint64_t session_uuid,
                                              uint16_t templateId,
                                              const char* data, size_t len) {
    switch (templateId) {
        case sbe::NewOrderSingle514::TEMPLATE_ID: {
            auto cmd = decodeNewOrderSingle(session_uuid, data, len);

            // Validate the order
            auto val_result = validator_.validateNewOrder(*cmd.order);
            if (!val_result.valid) {
                // We cannot enqueue a reject command here since we're on an IO
                // thread -- but we still need to notify the caller. For now we
                // mark the order as rejected via command type and let
                // processCommands handle it. However, the cleaner approach is to
                // stuff the reject info into the command so the engine thread
                // builds the reject response.
                //
                // To keep things simple and thread-safe, we still enqueue and
                // let the engine thread handle rejection.
                cmd.order->status = OrdStatus::Rejected;
            }

            // Rate check
            auto rate_result = risk_manager_.checkRate(session_uuid);
            if (!rate_result.passed) {
                cmd.order->status = OrdStatus::Rejected;
            }

            // Risk check
            auto risk_result = risk_manager_.checkOrder(*cmd.order);
            if (!risk_result.passed) {
                cmd.order->status = OrdStatus::Rejected;
            }

            command_queue_.push(std::move(cmd));
            break;
        }

        case sbe::OrderCancelRequest516::TEMPLATE_ID: {
            auto cmd = decodeCancelRequest(session_uuid, data, len);

            auto val_result = validator_.validateCancel(cmd.cancel_order_id, cmd.security_id);
            if (!val_result.valid) {
                // Still enqueue; processCommands will generate cancel reject
                cmd.type = OrderCommand::Type::CancelOrder;
            }

            command_queue_.push(std::move(cmd));
            break;
        }

        case sbe::OrderCancelReplaceRequest515::TEMPLATE_ID: {
            auto cmd = decodeModifyRequest(session_uuid, data, len);

            auto val_result = validator_.validateModify(
                cmd.cancel_order_id, cmd.security_id, cmd.new_price, cmd.new_qty);
            if (!val_result.valid) {
                // Still enqueue; processCommands will handle
            }

            command_queue_.push(std::move(cmd));
            break;
        }

        default:
            // Unknown template ID - ignore
            break;
    }
}

std::vector<OrderResponse> OrderEntryGateway::processCommands(
    IMatchingEngine& engine, std::vector<EngineEvent>* engine_events) {
    std::vector<OrderResponse> responses;

    while (auto cmd_opt = command_queue_.tryPop()) {
        auto& cmd = *cmd_opt;

        switch (cmd.type) {
            case OrderCommand::Type::NewOrder: {
                // If pre-rejected during validation (on IO thread)
                if (cmd.order->status == OrdStatus::Rejected) {
                    OrderRejected reject_event;
                    reject_event.cl_ord_id = cmd.order->cl_ord_id;
                    reject_event.session_uuid = cmd.session_uuid;
                    reject_event.reason = "Pre-trade risk check failed";
                    reject_event.reject_reason_code = 3; // Other

                    OrderResponse resp;
                    resp.session_uuid = cmd.session_uuid;
                    resp.sbe_message = exec_builder_.buildExecutionReportReject(
                        reject_event, cmd.session_uuid);
                    responses.push_back(std::move(resp));
                    break;
                }

                auto events = engine.submitOrder(std::move(cmd.order));
                if (engine_events) {
                    engine_events->insert(engine_events->end(), events.begin(), events.end());
                }
                for (const auto& event : events) {
                    // Determine which session(s) to route to
                    std::visit([&](const auto& e) {
                        using T = std::decay_t<decltype(e)>;
                        if constexpr (std::is_same_v<T, OrderAccepted>) {
                            OrderResponse resp;
                            resp.session_uuid = e.session_uuid;
                            resp.sbe_message = exec_builder_.buildExecutionReportNew(
                                e, e.session_uuid);
                            responses.push_back(std::move(resp));
                        } else if constexpr (std::is_same_v<T, OrderRejected>) {
                            OrderResponse resp;
                            resp.session_uuid = e.session_uuid;
                            resp.sbe_message = exec_builder_.buildExecutionReportReject(
                                e, e.session_uuid);
                            responses.push_back(std::move(resp));
                        } else if constexpr (std::is_same_v<T, OrderFilled>) {
                            // Send fill to maker
                            {
                                OrderResponse resp;
                                resp.session_uuid = e.maker_session_uuid;
                                resp.sbe_message = exec_builder_.buildExecutionReportFill(
                                    e, e.maker_session_uuid, true);
                                responses.push_back(std::move(resp));
                            }
                            // Send fill to taker
                            {
                                OrderResponse resp;
                                resp.session_uuid = e.taker_session_uuid;
                                resp.sbe_message = exec_builder_.buildExecutionReportFill(
                                    e, e.taker_session_uuid, false);
                                responses.push_back(std::move(resp));
                            }
                            // Update risk manager positions
                            risk_manager_.onFill(e.maker_session_uuid, e.security_id,
                                (e.aggressor_side == Side::Buy) ? Side::Sell : Side::Buy,
                                e.trade_qty);
                            risk_manager_.onFill(e.taker_session_uuid, e.security_id,
                                e.aggressor_side, e.trade_qty);
                        } else if constexpr (std::is_same_v<T, OrderCancelled>) {
                            OrderResponse resp;
                            resp.session_uuid = e.session_uuid;
                            resp.sbe_message = exec_builder_.buildExecutionReportCancel(
                                e, e.session_uuid);
                            responses.push_back(std::move(resp));
                        } else if constexpr (std::is_same_v<T, OrderModified>) {
                            OrderResponse resp;
                            resp.session_uuid = e.session_uuid;
                            resp.sbe_message = exec_builder_.buildExecutionReportModify(
                                e, e.session_uuid);
                            responses.push_back(std::move(resp));
                        } else if constexpr (std::is_same_v<T, OrderCancelRejected>) {
                            OrderResponse resp;
                            resp.session_uuid = e.session_uuid;
                            resp.sbe_message = exec_builder_.buildOrderCancelReject(
                                e, e.session_uuid);
                            responses.push_back(std::move(resp));
                        } else if constexpr (std::is_same_v<T, BookUpdate>) {
                            // BookUpdate is for market data, not order entry responses
                        }
                    }, event);
                }
                break;
            }

            case OrderCommand::Type::CancelOrder: {
                auto events = engine.cancelOrder(
                    cmd.cancel_order_id, cmd.security_id, cmd.session_uuid);
                if (engine_events) {
                    engine_events->insert(engine_events->end(), events.begin(), events.end());
                }
                for (const auto& event : events) {
                    std::visit([&](const auto& e) {
                        using T = std::decay_t<decltype(e)>;
                        if constexpr (std::is_same_v<T, OrderCancelled>) {
                            OrderResponse resp;
                            resp.session_uuid = e.session_uuid;
                            resp.sbe_message = exec_builder_.buildExecutionReportCancel(
                                e, e.session_uuid);
                            responses.push_back(std::move(resp));
                        } else if constexpr (std::is_same_v<T, OrderCancelRejected>) {
                            OrderResponse resp;
                            resp.session_uuid = e.session_uuid;
                            resp.sbe_message = exec_builder_.buildOrderCancelReject(
                                e, e.session_uuid);
                            responses.push_back(std::move(resp));
                        } else if constexpr (std::is_same_v<T, BookUpdate>) {
                            // Market data event, skip
                        } else {
                            // Unexpected event type for cancel
                        }
                    }, event);
                }
                break;
            }

            case OrderCommand::Type::ModifyOrder: {
                auto events = engine.modifyOrder(
                    cmd.cancel_order_id, cmd.security_id,
                    cmd.new_price, cmd.new_qty, cmd.new_cl_ord_id);
                if (engine_events) {
                    engine_events->insert(engine_events->end(), events.begin(), events.end());
                }
                for (const auto& event : events) {
                    std::visit([&](const auto& e) {
                        using T = std::decay_t<decltype(e)>;
                        if constexpr (std::is_same_v<T, OrderModified>) {
                            OrderResponse resp;
                            resp.session_uuid = e.session_uuid;
                            resp.sbe_message = exec_builder_.buildExecutionReportModify(
                                e, e.session_uuid);
                            responses.push_back(std::move(resp));
                        } else if constexpr (std::is_same_v<T, OrderCancelRejected>) {
                            OrderResponse resp;
                            resp.session_uuid = e.session_uuid;
                            resp.sbe_message = exec_builder_.buildOrderCancelReject(
                                e, e.session_uuid);
                            responses.push_back(std::move(resp));
                        } else if constexpr (std::is_same_v<T, OrderFilled>) {
                            // Modify can trigger fills
                            {
                                OrderResponse resp;
                                resp.session_uuid = e.maker_session_uuid;
                                resp.sbe_message = exec_builder_.buildExecutionReportFill(
                                    e, e.maker_session_uuid, true);
                                responses.push_back(std::move(resp));
                            }
                            {
                                OrderResponse resp;
                                resp.session_uuid = e.taker_session_uuid;
                                resp.sbe_message = exec_builder_.buildExecutionReportFill(
                                    e, e.taker_session_uuid, false);
                                responses.push_back(std::move(resp));
                            }
                            risk_manager_.onFill(e.maker_session_uuid, e.security_id,
                                (e.aggressor_side == Side::Buy) ? Side::Sell : Side::Buy,
                                e.trade_qty);
                            risk_manager_.onFill(e.taker_session_uuid, e.security_id,
                                e.aggressor_side, e.trade_qty);
                        } else if constexpr (std::is_same_v<T, BookUpdate>) {
                            // Market data event, skip
                        } else {
                            // Other events not expected for modify
                        }
                    }, event);
                }
                break;
            }
        }
    }

    return responses;
}

bool OrderEntryGateway::hasPendingCommands() const {
    return !command_queue_.empty();
}

OrderResponse OrderEntryGateway::buildResponse(const EngineEvent& event,
                                                uint64_t session_uuid) {
    OrderResponse resp;
    resp.session_uuid = session_uuid;
    resp.sbe_message = exec_builder_.buildFromEvent(event, session_uuid);
    return resp;
}

OrderCommand OrderEntryGateway::decodeNewOrderSingle(uint64_t session_uuid,
                                                      const char* data, size_t len) {
    OrderCommand cmd;
    cmd.type = OrderCommand::Type::NewOrder;
    cmd.session_uuid = session_uuid;

    sbe::NewOrderSingle514 sbe_msg;
    sbe_msg.decode(data, 0);

    auto order = std::make_unique<Order>();
    order->session_uuid = session_uuid;
    order->security_id = sbe_msg.securityID;
    order->side = static_cast<Side>(sbe_msg.side);
    order->order_type = static_cast<OrderType>(sbe_msg.ordType);
    order->time_in_force = static_cast<TimeInForce>(sbe_msg.timeInForce);
    order->price = Price{sbe_msg.price};
    order->stop_price = Price{sbe_msg.stopPx};
    order->quantity = static_cast<Quantity>(sbe_msg.orderQty);
    order->display_qty = static_cast<Quantity>(sbe_msg.displayQty);
    order->min_qty = static_cast<Quantity>(sbe_msg.minQty);
    order->order_request_id = sbe_msg.orderRequestID;

    // Read ClOrdID from fixed-size field
    char clOrdBuf[21]{};
    sbe::readFixedString(clOrdBuf, sbe_msg.clOrdID, 20);
    order->cl_ord_id = clOrdBuf;

    cmd.order = std::move(order);
    cmd.security_id = sbe_msg.securityID;

    return cmd;
}

OrderCommand OrderEntryGateway::decodeCancelRequest(uint64_t session_uuid,
                                                     const char* data, size_t len) {
    OrderCommand cmd;
    cmd.type = OrderCommand::Type::CancelOrder;
    cmd.session_uuid = session_uuid;

    sbe::OrderCancelRequest516 sbe_msg;
    sbe_msg.decode(data, 0);

    cmd.cancel_order_id = sbe_msg.orderID;
    cmd.security_id = sbe_msg.securityID;
    cmd.order_request_id = sbe_msg.orderRequestID;

    char clOrdBuf[21]{};
    sbe::readFixedString(clOrdBuf, sbe_msg.clOrdID, 20);
    cmd.cl_ord_id = clOrdBuf;

    return cmd;
}

OrderCommand OrderEntryGateway::decodeModifyRequest(uint64_t session_uuid,
                                                     const char* data, size_t len) {
    OrderCommand cmd;
    cmd.type = OrderCommand::Type::ModifyOrder;
    cmd.session_uuid = session_uuid;

    sbe::OrderCancelReplaceRequest515 sbe_msg;
    sbe_msg.decode(data, 0);

    cmd.cancel_order_id = sbe_msg.orderID;
    cmd.security_id = sbe_msg.securityID;
    cmd.new_price = Price{sbe_msg.price};
    cmd.new_qty = static_cast<Quantity>(sbe_msg.orderQty);
    cmd.order_request_id = sbe_msg.orderRequestID;

    char clOrdBuf[21]{};
    sbe::readFixedString(clOrdBuf, sbe_msg.clOrdID, 20);
    cmd.cl_ord_id = clOrdBuf;
    cmd.new_cl_ord_id = clOrdBuf;

    return cmd;
}

OrderResponse OrderEntryGateway::buildPreEngineReject(uint64_t session_uuid,
                                                       const ClOrdId& cl_ord_id,
                                                       SecurityId security_id,
                                                       uint16_t reject_reason,
                                                       const std::string& reason) {
    OrderRejected reject_event;
    reject_event.cl_ord_id = cl_ord_id;
    reject_event.session_uuid = session_uuid;
    reject_event.reason = reason;
    reject_event.reject_reason_code = reject_reason;

    OrderResponse resp;
    resp.session_uuid = session_uuid;
    resp.sbe_message = exec_builder_.buildExecutionReportReject(
        reject_event, session_uuid);
    return resp;
}

} // namespace cme::sim::gateway
