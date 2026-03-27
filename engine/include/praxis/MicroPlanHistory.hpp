#pragma once
#include <string>
#include <vector>
#include <map>
#include "PlanParser.hpp"
#include "PatchParser.hpp"

namespace praxis {
namespace plan {

// Micro version entry for simple plan step history
struct MicroPlanVersion {
    int version = 1;
    int parent_version = -1;
    std::vector<ParsedPlanStep> steps;
    std::string reason;
    std::string created_at; // ISO-8601
    std::string steps_hash; // SHA256 of canonical step JSON
};

// Lightweight plan history for micro-intent tracking
class MicroPlanHistory {
public:
    MicroPlanHistory() = default;

    // Create v1 from parsed plan
    int create_initial(const std::string& plan_id, const ParsedPlan& plan, const std::string& reason = "Initial plan");

    // Apply parsed patch operations to latest version, create new version
    int apply_patch(const std::string& plan_id, const ParsedPatch& patch);

    // Get specific version
    std::vector<ParsedPlanStep> get_version_steps(const std::string& plan_id, int version) const;

    // Get latest version number
    int get_latest_version(const std::string& plan_id) const;

    // Write ledger JSON to file
    bool write_ledger(const std::string& plan_id, const std::string& path) const;

private:
    std::map<std::string, std::vector<MicroPlanVersion>> history_; // plan_id -> versions

    std::string compute_steps_hash(const std::vector<ParsedPlanStep>& steps) const;
    std::string generate_timestamp() const;
};

} // namespace plan
} // namespace praxis
