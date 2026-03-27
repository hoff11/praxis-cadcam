#pragma once
#include <string>
#include <vector>
#include <optional>
#include <map>

namespace praxis {
namespace plan {

struct ParsedPlanStep {
    std::string op_type;
    std::map<std::string, std::string> params; // stringified args for IntentRequest
};

struct ParsedPlan {
    int version = 1;
    std::string units = "mm";
    std::vector<ParsedPlanStep> steps;
    std::string plan_id; // optional; empty if not provided
};

// Parse canonical Plan JSON into a minimal internal format
std::optional<ParsedPlan> parsePlanJson(const std::string& jsonText);

} // namespace plan
} // namespace praxis
