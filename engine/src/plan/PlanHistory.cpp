#include "../../include/praxis/PlanHistory.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace praxis {
namespace plan {

std::string PlanHistory::generate_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
    #ifdef _WIN32
    gmtime_s(&tm_utc, &now_t);
    #else
    gmtime_r(&now_t, &tm_utc);
    #endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

int PlanHistory::create_initial_version(
    const std::string& plan_id,
    const recipe::ResolvedPlan& resolved_plan,
    const std::string& reason
) {
    if (plan_exists(plan_id)) {
        throw std::runtime_error("Plan already exists: " + plan_id);
    }

    PlanVersion v1;
    v1.plan_id = plan_id;
    v1.version = 1;
    v1.parent_version = -1;  // Root version
    v1.resolved_plan = resolved_plan;

    // Ensure initial version has a computed plan hash like patched versions do.
    auto hash_validation = recipe::ResolvedPlan::validate_for_hash(v1.resolved_plan);
    if (hash_validation.valid) {
        v1.resolved_plan.plan_hash = v1.resolved_plan.compute_hash();
        v1.resolved_plan.hash_valid = true;
        v1.resolved_plan.hash_status_reason.clear();
    } else {
        v1.resolved_plan.plan_hash.clear();
        v1.resolved_plan.hash_valid = false;
        v1.resolved_plan.hash_status_reason = hash_validation.reason;
    }

    v1.created_at = generate_timestamp();
    v1.reason = reason;

    versions_[plan_id][1] = v1;
    return 1;
}

bool PlanHistory::plan_exists(const std::string& plan_id) const {
    return versions_.find(plan_id) != versions_.end();
}

std::optional<PlanVersion> PlanHistory::get_version(
    const std::string& plan_id, 
    int version
) const {
    auto plan_it = versions_.find(plan_id);
    if (plan_it == versions_.end()) {
        return std::nullopt;
    }

    auto version_it = plan_it->second.find(version);
    if (version_it == plan_it->second.end()) {
        return std::nullopt;
    }

    return version_it->second;
}

int PlanHistory::get_latest_version(const std::string& plan_id) const {
    auto plan_it = versions_.find(plan_id);
    if (plan_it == versions_.end() || plan_it->second.empty()) {
        return -1;
    }

    // Return max version number
    return plan_it->second.rbegin()->first;
}

std::vector<int> PlanHistory::list_versions(const std::string& plan_id) const {
    auto plan_it = versions_.find(plan_id);
    if (plan_it == versions_.end()) {
        return {};
    }

    std::vector<int> result;
    for (const auto& [version, _] : plan_it->second) {
        result.push_back(version);
    }
    return result;
}

std::vector<std::string> PlanHistory::list_plans() const {
    std::vector<std::string> result;
    for (const auto& [plan_id, _] : versions_) {
        result.push_back(plan_id);
    }
    return result;
}

PatchValidationResult PlanHistory::validate_operation(
    const PatchOperation& op,
    const PlanVersion& target_version
) const {
    PatchValidationResult result;
    const auto& steps = target_version.resolved_plan.operations;

    switch (op.op_type) {
        case PatchOpType::AddStep:
            if (!op.new_step.has_value()) {
                result.error_message = "AddStep operation missing new_step";
                return result;
            }
            // AddStep is always valid (appends to end)
            result.valid = true;
            break;

        case PatchOpType::ModifyStep:
            if (op.step_index < 0 || op.step_index >= static_cast<int>(steps.size())) {
                result.error_message = "ModifyStep step_index out of bounds: " + 
                    std::to_string(op.step_index) + " (plan has " + 
                    std::to_string(steps.size()) + " steps)";
                return result;
            }
            if (!op.new_step.has_value()) {
                result.error_message = "ModifyStep operation missing new_step";
                return result;
            }
            result.valid = true;
            break;

        case PatchOpType::RemoveStep:
            // EPIC 1.6.3: RemoveStep now allowed - validate bounds
            if (op.step_index < 0 || op.step_index >= static_cast<int>(steps.size())) {
                result.error_message = "RemoveStep step_index out of bounds: " + 
                                     std::to_string(op.step_index) + 
                                     " (plan has " + std::to_string(steps.size()) + " steps)";
                return result;
            }
            result.valid = true;
            break;
    }

    return result;
}

PatchValidationResult PlanHistory::validate_patch(const PlanPatch& patch) const {
    PatchValidationResult result;

    // Check plan exists
    if (!plan_exists(patch.plan_id)) {
        result.error_message = "Plan does not exist: " + patch.plan_id;
        return result;
    }

    // Check target version exists
    auto target_version_opt = get_version(patch.plan_id, patch.target_version);
    if (!target_version_opt.has_value()) {
        result.error_message = "Target version " + std::to_string(patch.target_version) + 
            " does not exist for plan " + patch.plan_id;
        return result;
    }

    const auto& target_version = target_version_opt.value();

    // Check patch has at least one operation
    if (patch.operations.empty()) {
        result.error_message = "Patch must specify at least one operation";
        return result;
    }

    // Validate each operation
    for (const auto& op : patch.operations) {
        auto op_validation = validate_operation(op, target_version);
        if (!op_validation.valid) {
            result.error_message = op_validation.error_message;
            result.warnings = op_validation.warnings;
            return result;
        }
    }

    result.valid = true;
    return result;
}

std::vector<recipe::ResolvedOperation> PlanHistory::apply_operation(
    const PatchOperation& op,
    const std::vector<recipe::ResolvedOperation>& existing_steps
) const {
    std::vector<recipe::ResolvedOperation> new_steps = existing_steps;

    switch (op.op_type) {
        case PatchOpType::AddStep:
            new_steps.push_back(op.new_step.value());
            break;

        case PatchOpType::ModifyStep:
            new_steps[op.step_index] = op.new_step.value();
            break;

        case PatchOpType::RemoveStep:
            // EPIC 1.6.3: Remove step at specified index
            if (op.step_index >= 0 && op.step_index < static_cast<int>(new_steps.size())) {
                new_steps.erase(new_steps.begin() + op.step_index);
            } else {
                throw std::runtime_error("Invalid step_index in RemoveStep");
            }
            break;
    }

    return new_steps;
}

PatchApplicationResult PlanHistory::apply_patch(const PlanPatch& patch) {
    PatchApplicationResult result;

    // Validate patch
    auto validation = validate_patch(patch);
    if (!validation.valid) {
        result.error_message = validation.error_message;
        return result;
    }

    // Get target version
    auto target_version = get_version(patch.plan_id, patch.target_version).value();

    // Apply operations to create new steps
    std::vector<recipe::ResolvedOperation> new_steps = 
        target_version.resolved_plan.operations;

    for (const auto& op : patch.operations) {
        new_steps = apply_operation(op, new_steps);
    }

    // Create new version
    int new_version_num = get_latest_version(patch.plan_id) + 1;

    PlanVersion new_version;
    new_version.plan_id = patch.plan_id;
    new_version.version = new_version_num;
    new_version.parent_version = patch.target_version;
    new_version.created_at = generate_timestamp();
    new_version.reason = patch.reason;

    // Copy resolved plan and update operations
    new_version.resolved_plan = target_version.resolved_plan;
    new_version.resolved_plan.operations = new_steps;

    // Recompute plan hash for new version
    auto hash_validation = recipe::ResolvedPlan::validate_for_hash(new_version.resolved_plan);
    if (hash_validation.valid) {
        new_version.resolved_plan.plan_hash = new_version.resolved_plan.compute_hash();
        new_version.resolved_plan.hash_valid = true;
        new_version.resolved_plan.hash_status_reason = "";
    } else {
        new_version.resolved_plan.plan_hash = "";
        new_version.resolved_plan.hash_valid = false;
        new_version.resolved_plan.hash_status_reason = hash_validation.reason;
    }

    // Store new version
    versions_[patch.plan_id][new_version_num] = new_version;

    // Return success
    result.success = true;
    result.new_version = new_version_num;
    result.new_plan = new_version;

    return result;
}

} // namespace plan
} // namespace praxis
