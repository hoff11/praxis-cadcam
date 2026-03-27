#include "praxis/PatchParser.hpp"
#include <nlohmann/json.hpp>

namespace praxis {
namespace plan {

std::optional<ParsedPatch> parsePatchJson(const std::string& jsonText) {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonText);
        ParsedPatch patch;
        if (j.contains("plan_id") && j["plan_id"].is_string()) patch.plan_id = j["plan_id"].get<std::string>();
        if (j.contains("target_version") && j["target_version"].is_number_integer()) patch.target_version = j["target_version"].get<int>();
        if (j.contains("reason") && j["reason"].is_string()) patch.reason = j["reason"].get<std::string>();
        if (!j.contains("operations") || !j["operations"].is_array()) return std::nullopt;
        for (const auto& opj : j["operations"]) {
            if (!opj.contains("op_type") || !opj["op_type"].is_string()) continue;
            ParsedPatchOperation op; op.op_type = opj["op_type"].get<std::string>();
            if (opj.contains("step_index") && opj["step_index"].is_number_integer()) op.step_index = opj["step_index"].get<int>();
            if (opj.contains("reason") && opj["reason"].is_string()) op.reason = opj["reason"].get<std::string>();
            if (opj.contains("new_step") && opj["new_step"].is_object()) {
                ParsedPlanStep ps;
                if (opj["new_step"].contains("op_type") && opj["new_step"]["op_type"].is_string()) ps.op_type = opj["new_step"]["op_type"].get<std::string>();
                if (opj["new_step"].contains("args") && opj["new_step"]["args"].is_object()) {
                    for (auto it = opj["new_step"]["args"].begin(); it != opj["new_step"]["args"].end(); ++it) {
                        const std::string key = it.key();
                        const auto& val = it.value();
                        if (val.is_number()) {
                            ps.params[key] = std::to_string(val.get<double>());
                        } else if (val.is_string()) {
                            ps.params[key] = val.get<std::string>();
                        } else if (val.is_boolean()) {
                            ps.params[key] = val.get<bool>() ? "true" : "false";
                        }
                    }
                }
                op.new_step = ps;
            }
            patch.operations.push_back(op);
        }
        if (patch.operations.empty()) return std::nullopt;
        return patch;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace plan
} // namespace praxis
