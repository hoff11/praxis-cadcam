#pragma once
#include <string>
#include <vector>
#include <optional>
#include <map>
#include "PlanParser.hpp"

namespace praxis {
namespace plan {

struct ParsedPatchOperation {
    std::string op_type; // AddStep | ModifyStep | RemoveStep
    int step_index = -1; // for modify/remove
    std::optional<ParsedPlanStep> new_step; // for add/modify
    std::string reason;
};

struct ParsedPatch {
    std::string plan_id;
    int target_version = 1;
    std::string reason;
    std::vector<ParsedPatchOperation> operations;
};

// Parse canonical PlanPatch JSON into minimal internal structures
std::optional<ParsedPatch> parsePatchJson(const std::string& jsonText);

} // namespace plan
} // namespace praxis
