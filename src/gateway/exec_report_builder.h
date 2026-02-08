#pragma once

#include "../common/types.h"
#include "../engine/engine_event.h"
#include <vector>
#include <string>
#include <cstdint>

namespace cme::sim::gateway {

class ExecReportBuilder {
public:
    std::vector<char> buildExecutionReportNew(const OrderAccepted& event, uint64_t uuid);
    std::vector<char> buildExecutionReportReject(const OrderRejected& event, uint64_t uuid);
    std::vector<char> buildExecutionReportFill(const OrderFilled& event, uint64_t uuid, bool is_maker);
    std::vector<char> buildExecutionReportCancel(const OrderCancelled& event, uint64_t uuid);
    std::vector<char> buildExecutionReportModify(const OrderModified& event, uint64_t uuid);
    std::vector<char> buildExecutionReportElimination(const OrderAccepted& event, uint64_t uuid);
    std::vector<char> buildOrderCancelReject(const OrderCancelRejected& event, uint64_t uuid);

    std::vector<char> buildFromEvent(const EngineEvent& event, uint64_t session_uuid);

private:
    uint64_t next_exec_id_ = 1;
    std::string generateExecId();
};

} // namespace cme::sim::gateway
