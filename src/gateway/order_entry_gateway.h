#pragma once

#include "../common/types.h"
#include "../common/mpsc_queue.h"
#include "../engine/matching_engine.h"
#include "../engine/order.h"
#include "../engine/engine_event.h"
#include "../instruments/instrument_manager.h"
#include "../config/exchange_config.h"
#include "message_validator.h"
#include "risk_manager.h"
#include "exec_report_builder.h"
#include <memory>
#include <functional>
#include <vector>

namespace cme::sim::gateway {

struct OrderCommand {
    enum class Type { NewOrder, CancelOrder, ModifyOrder };
    Type type = Type::NewOrder;
    uint64_t session_uuid = 0;

    // NewOrder fields
    std::unique_ptr<Order> order;

    // Cancel fields
    OrderId cancel_order_id = 0;
    SecurityId security_id = 0;
    ClOrdId cl_ord_id;
    uint64_t order_request_id = 0;

    // Modify fields
    Price new_price;
    Quantity new_qty = 0;
    ClOrdId new_cl_ord_id;

    // Move-only due to unique_ptr
    OrderCommand() = default;
    OrderCommand(OrderCommand&&) = default;
    OrderCommand& operator=(OrderCommand&&) = default;
};

struct OrderResponse {
    uint64_t session_uuid;
    std::vector<char> sbe_message;
};

class OrderEntryGateway {
public:
    OrderEntryGateway(InstrumentManager& instrument_mgr,
                      const config::RiskConfig& risk_config);

    // Called by FIXP session when app message received (on IO thread)
    // Decodes SBE, validates, enqueues to engine
    void onApplicationMessage(uint64_t session_uuid, uint16_t templateId,
                              const char* data, size_t len);

    // Called by engine thread to process commands
    // Returns responses to route back to sessions
    // If engine_events is non-null, all raw engine events are appended for market data
    std::vector<OrderResponse> processCommands(IMatchingEngine& engine,
                                               std::vector<EngineEvent>* engine_events = nullptr);

    // Check for pending commands
    bool hasPendingCommands() const;

    // Build execution report from engine event and route to session
    OrderResponse buildResponse(const EngineEvent& event, uint64_t session_uuid);

private:
    InstrumentManager& instrument_mgr_;
    MessageValidator validator_;
    RiskManager risk_manager_;
    ExecReportBuilder exec_builder_;

    MPSCQueue<OrderCommand> command_queue_;

    // Decode handlers
    OrderCommand decodeNewOrderSingle(uint64_t session_uuid, const char* data, size_t len);
    OrderCommand decodeCancelRequest(uint64_t session_uuid, const char* data, size_t len);
    OrderCommand decodeModifyRequest(uint64_t session_uuid, const char* data, size_t len);

    // Build a reject response before submitting to engine
    OrderResponse buildPreEngineReject(uint64_t session_uuid, const ClOrdId& cl_ord_id,
                                       SecurityId security_id, uint16_t reject_reason,
                                       const std::string& reason);
};

} // namespace cme::sim::gateway
