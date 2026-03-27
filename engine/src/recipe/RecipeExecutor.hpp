#pragma once
#include "RecipeTypes.hpp"
#include "../kernel/KernelOps.hpp"
#include "../kinematics/KinematicTypes.hpp"
#include "../reasoning/ReasoningTypes.hpp"
#include "../../include/praxis/Intent.hpp"
#include <map>
#include <string>

namespace praxis {
namespace recipe {

// Node binding information (maps node ID to body)
struct NodeBodyBinding {
    std::string node_id;
    std::string body_id;
    std::string frame_id;  // Optional
};

// Node execution status (Sprint 6 Phase 3)
struct NodeExecutionStatus {
    std::string node_id;
    size_t node_index;
    std::string op_type;
    std::string op_hash;
    std::string status;  // "executed" | "reused" | "failed"
    bool cache_hit = false;
    std::string artifact_path;  // Legacy: primary output (for single-output ops)
    std::map<std::string, std::string> outputs;  // P1-4: Named outputs ("out" -> path)
};

// Recipe execution result
struct RecipeExecutionResult {
    std::string report_version = "0.4";  // Bumped for Sprint 6 Phase 3
    bool success = true;
    std::string error_message;
    std::vector<std::string> warnings;
    
    // Output artifacts
    std::string step_file_path;
    std::string pkm_file_path;
    std::string bindings_file_path;
    
    // Execution trace
    std::vector<std::string> kernel_ops;
    std::map<std::string, std::string> metrics;
    
    // Bindings
    std::vector<NodeBodyBinding> bindings;
    
    // Reasoning notes (Sprint 3)
    std::vector<reasoning::ReasoningNote> reasoning;
    
    // Cache info (Sprint 5 Phase 4 - plan-level)
    struct CacheInfo {
        bool hit = false;
        std::string plan_hash = "";
        std::string reason = "";
        std::vector<std::string> reused_artifacts;
        int64_t cache_entry_age_seconds = 0;
    } cache;
    
    // Op-level execution status (Sprint 6 Phase 3)
    struct OpCacheInfo {
        int executed_count = 0;
        int reused_count = 0;
        double cache_hit_rate = 0.0;
        std::string cache_dir;  // Absolute cache root for path normalization
        std::vector<NodeExecutionStatus> node_status;
    } op_cache;
    
    // Resolved inputs (Sprint 3 EPIC 3)
    std::map<std::string, double> resolved_params;
    std::map<std::string, double> derived_values;
    
    // Resolved plan (Sprint 5 Phase 1)
    ResolvedPlan resolved_plan;
    
    // Sprint 10 Task 3.1: Structured result summary
    struct Summary {
        std::string intent;                      // Human-readable intent description
        std::vector<std::string> produced;       // Semantic objects produced (ordered deterministically)
        int body_count = 0;
        int face_count = 0;
        int edge_count = 0;
        int artifact_count = 0;
        std::vector<std::string> previewable_outputs;  // List of previewable artifact names
    } summary;
};

class RecipeExecutor {
public:
    // Execute a recipe to produce geometry + PKM + bindings
    static RecipeExecutionResult execute(
        const Recipe& recipe,
        const kinematics::PKM& pkm,
        const std::map<std::string, std::string>& cli_params,
        const std::string& output_dir,
        const std::vector<RecipeInclude>& all_includes = {},
        bool no_cache = false,
        bool clear_cache = false
    );
    
    // Validate params/variants/derived/bindings without executing kernel ops or writing artifacts
    static RecipeExecutionResult validate_only(
        const Recipe& recipe,
        const kinematics::PKM& pkm,
        const std::map<std::string, std::string>& cli_params
    );
    
private:
    // Merge default params with CLI overrides
    static std::map<std::string, double> merge_params(
        const Recipe& recipe,
        const std::map<std::string, std::string>& cli_params,
        std::vector<std::string>& errors
    );
    
    // Evaluate derived expressions
    static std::map<std::string, double> evaluate_derived(
        const Recipe& recipe,
        const std::map<std::string, double>& params,
        std::vector<std::string>& errors
    );
    
    // Execute a single node
    static kernel::KernelOpResult execute_node(
        const RecipeNode& node,
        const std::map<std::string, double>& all_values,
        std::map<std::string, kernel::ShapeHandle>& node_outputs
    );
    
    // Validate bindings against PKM bodies
    static bool validate_bindings(
        const Recipe& recipe,
        const kinematics::PKM& pkm,
        std::vector<std::string>& errors
    );
    
    // Write bindings.json
    static bool write_bindings_file(
        const std::vector<NodeBodyBinding>& bindings,
        const std::string& file_path
    );
    
    // Build resolved plan (Sprint 5 Phase 1)
    static ResolvedPlan build_resolved_plan(
        const Recipe& recipe,
        const std::map<std::string, double>& final_params,
        const std::vector<RecipeInclude>& all_includes,
        const std::string& kinematics_path
    );
    
    // Sprint 10 Task 3.1: Generate structured result summary
    static void generate_result_summary(
        RecipeExecutionResult& result,
        const Recipe& recipe
    );
};

} // namespace recipe
} // namespace praxis
