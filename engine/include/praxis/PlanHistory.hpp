#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include "../../src/recipe/RecipeTypes.hpp"

namespace praxis {
namespace plan {

// EPIC 1.3: Plan version with immutability tracking
struct PlanVersion {
    std::string plan_id;
    int version;
    int parent_version;  // -1 for root version
    recipe::ResolvedPlan resolved_plan;
    std::string created_at;  // ISO-8601 timestamp
    std::string reason;      // Why this version was created
};

// Patch operation types
enum class PatchOpType {
    AddStep,
    ModifyStep,
    RemoveStep
};

// Single patch operation
struct PatchOperation {
    PatchOpType op_type;
    int step_index;  // For modify/remove, index in target version
    std::optional<recipe::ResolvedOperation> new_step;  // For add/modify
    std::string reason;
};

// Plan patch (creates new version from existing)
struct PlanPatch {
    std::string plan_id;
    int target_version;
    std::vector<PatchOperation> operations;
    std::string reason;  // Overall reason for patch
};

// Result of patch validation
struct PatchValidationResult {
    bool valid = false;
    std::string error_message;
    std::vector<std::string> warnings;
};

// Result of patch application
struct PatchApplicationResult {
    bool success = false;
    std::string error_message;
    int new_version = -1;  // Version number of newly created plan
    PlanVersion new_plan;  // The resulting plan version (if success)
};

// Plan history manager (EPIC 1.3)
class PlanHistory {
public:
    PlanHistory() = default;

    // Create initial plan version (v1)
    // Returns version number (1)
    int create_initial_version(
        const std::string& plan_id,
        const recipe::ResolvedPlan& resolved_plan,
        const std::string& reason = "Initial plan"
    );

    // Apply patch to create new version
    // Validates patch, checks immutability rules, creates new version
    // Returns result with new version number or error
    PatchApplicationResult apply_patch(const PlanPatch& patch);

    // Get specific plan version
    std::optional<PlanVersion> get_version(const std::string& plan_id, int version) const;

    // Get latest version number for plan
    int get_latest_version(const std::string& plan_id) const;

    // List all versions for a plan
    std::vector<int> list_versions(const std::string& plan_id) const;

    // Validate patch without applying
    PatchValidationResult validate_patch(const PlanPatch& patch) const;

    // Check if plan exists
    bool plan_exists(const std::string& plan_id) const;

    // Get all plan IDs
    std::vector<std::string> list_plans() const;

private:
    // Storage: map of (plan_id -> map of (version -> PlanVersion))
    std::map<std::string, std::map<int, PlanVersion>> versions_;

    // Helper: validate patch operation
    PatchValidationResult validate_operation(
        const PatchOperation& op,
        const PlanVersion& target_version
    ) const;

    // Helper: apply single operation to create new steps
    std::vector<recipe::ResolvedOperation> apply_operation(
        const PatchOperation& op,
        const std::vector<recipe::ResolvedOperation>& existing_steps
    ) const;

    // Helper: generate timestamp
    std::string generate_timestamp() const;
};

} // namespace plan
} // namespace praxis
