#include <string>
#include <vector>
#include <optional>
#include <map>
#include <regex>
#include <sstream>
#include "praxis/PlanParser.hpp"
#include <nlohmann/json.hpp>

namespace praxis {
namespace plan {

// Minimal Phase A parser for @praxis/types Plan JSON
// Supports: version (int), units ("mm"|"inch"), steps with op_type and flat numeric/string args

// Definitions are in header; implement parser here

std::optional<ParsedPlan> parsePlanJson(const std::string& jsonText) {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonText);
        ParsedPlan plan;
        if (j.contains("plan_id") && j["plan_id"].is_string()) plan.plan_id = j["plan_id"].get<std::string>();
        if (j.contains("version") && j["version"].is_number_integer()) plan.version = j["version"].get<int>();
        if (j.contains("units") && j["units"].is_string()) plan.units = j["units"].get<std::string>();
        if (!j.contains("steps") || !j["steps"].is_array()) return std::nullopt;
        for (const auto& stepj : j["steps"]) {
            if (!stepj.contains("op_type") || !stepj["op_type"].is_string()) continue;
            ParsedPlanStep step; step.op_type = stepj["op_type"].get<std::string>();
            if (stepj.contains("args") && stepj["args"].is_object()) {
                for (auto it = stepj["args"].begin(); it != stepj["args"].end(); ++it) {
                    const std::string key = it.key();
                    const auto& val = it.value();
                    if (val.is_number()) {
                        step.params[key] = std::to_string(val.get<double>());
                    } else if (val.is_string()) {
                        step.params[key] = val.get<std::string>();
                    } else if (val.is_boolean()) {
                        step.params[key] = val.get<bool>() ? "true" : "false";
                    } // Nested objects/arrays ignored in Phase B demo
                }
            }
            plan.steps.push_back(step);
        }
        if (plan.steps.empty()) return std::nullopt;
        return plan;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace plan
} // namespace praxis
