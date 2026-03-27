#pragma once
#include <string>
#include <vector>
#include <map>
#include <variant>

namespace praxis {
namespace recipe {

// Parameter definition with range and default
struct ParamDef {
    std::string name;
    double default_value;
    double min_value;
    double max_value;
    std::string description;
};

// Expression for derived values (v0: simple math only)
struct Expression {
    std::string formula;  // e.g., "width + 100"
    
    // Evaluate expression given parameter values
    double evaluate(const std::map<std::string, double>& values) const;
};

// Scalar value: either literal number or expression to evaluate
using Scalar = std::variant<double, Expression>;

// Helper to evaluate Scalar to double
double evaluate_scalar(const Scalar& scalar, const std::map<std::string, double>& values);

// Node operation types (v0 limited set)
enum class NodeOp {
    MakeBox,
    Transform,
    Compound
};

// Node binding to kinematic body
struct NodeBinding {
    std::string body;        // Required: target body name
    std::string frame = "";  // Optional: named frame (v0 can be empty)
};

// Arguments for different operations
struct MakeBoxArgs {
    Scalar dx, dy, dz;
};

struct TransformArgs {
    std::string input;       // Required: node ID to transform
    Scalar tx, ty, tz;
    Scalar rx, ry, rz;
};

struct CompoundArgs {
    std::vector<std::string> node_refs;  // References to other node IDs
};

// Union of possible arguments
using NodeArgs = std::variant<MakeBoxArgs, TransformArgs, CompoundArgs>;

// Operation source tracking (Sprint 4 provenance)
struct OperationSource {
    std::string recipe_path_canonical = "";  // Canonical absolute path for tracking
    std::string recipe_path_display = "";    // Display path (relative, for reports)
    std::string namespace_alias = "";        // Namespace (empty = root recipe)
    int operation_index = -1;                // Index in originating recipe's nodes array (-1 = unset)
};

// Recipe node (one operation in the recipe)
struct RecipeNode {
    std::string id;           // Unique node identifier
    NodeOp op;                // Operation type
    NodeArgs args;            // Operation-specific arguments
    NodeBinding binding;      // Required binding to kinematic body
    std::string description;  // Optional documentation
    OperationSource source;   // Provenance tracking (Sprint 4)
};

// Variant definition (override set)
struct Variant {
    std::string name;
    std::map<std::string, double> param_overrides;
};

// Recipe include for composition (Sprint 3 EPIC 2)
struct RecipeInclude {
    std::string recipe_path;  // Relative path to included recipe
    std::map<std::string, double> param_overrides;  // Override included recipe's defaults
    std::string namespace_alias = "";  // Optional namespace for isolation (Sprint 4 Phase 2)
};

// Resolved operation (Sprint 5 Phase 1)
struct ResolvedOperation {
    NodeOp op;                                     // Operation type
    std::string node_id;                           // Node identifier
    std::map<std::string, double> resolved_args;   // Fully resolved numeric arguments
    OperationSource source;                        // Provenance info
};

// Hash validation result (Sprint 5 Phase 3)
struct HashValidationResult {
    bool valid = false;
    std::string reason;  // Why validation failed (empty if valid)
};

// Resolved plan (Sprint 5 Phase 1)
struct ResolvedPlan {
    std::vector<ResolvedOperation> operations;     // Flattened, ordered operations
    std::vector<RecipeInclude> includes;           // All includes with canonical paths
    std::map<std::string, double> final_params;    // Final resolved parameter table (root + namespaces)
    std::string engine_version;                    // Engine version identifier
    std::string kernel_version;                    // Kernel version identifier (if available)
    std::string kinematics_path_canonical;         // Canonical path to PKM file
    std::string kinematics_sha256;                 // SHA256 of kinematics file content (Sprint 5 Phase 3)
    std::string output_node;                       // Output node ID
    std::string plan_hash;                         // SHA256 hash of canonical JSON (Sprint 5 Phase 2)
    bool hash_valid = false;                       // Whether plan_hash is trustworthy (Sprint 5 Phase 3)
    std::string hash_status_reason;                // Why hash is invalid (empty if valid)
    
    // Serialize to canonical JSON (deterministic, sorted keys)
    std::string to_canonical_json() const;
    
    // Compute SHA256 hash of canonical JSON
    std::string compute_hash() const;
    
    // Validate plan is eligible for hashing (Sprint 5 Phase 3)
    static HashValidationResult validate_for_hash(const ResolvedPlan& plan);
};

// Recipe (complete definition)
struct Recipe {
    std::string recipe_version = "0.1";  // Recipe format version
    std::string id;                      // Recipe identifier
    std::string units;                   // "mm" only for v0
    
    std::map<std::string, ParamDef> params;     // Parameter definitions
    std::map<std::string, Expression> derived;  // Derived value expressions
    std::vector<RecipeNode> nodes;              // Ordered operations
    std::map<std::string, Variant> variants;    // Named variants
    std::vector<RecipeInclude> includes;        // Included recipes (Sprint 3 EPIC 2)
    
    std::string kinematics_path;  // Relative path to PKM file (required)
    std::string output;           // Node ID to export as final STEP (required)
    
    // Metadata
    std::map<std::string, std::string> metadata;
};

// Recipe load result
struct RecipeLoadResult {
    bool success = true;
    Recipe recipe;
    std::string error_message;
    std::vector<std::string> warnings;
    std::vector<RecipeInclude> all_includes_used;  // Flattened list of all includes (Sprint 4)
};

// Recipe validation result
struct RecipeValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// Helper functions
std::string node_op_to_string(NodeOp op);
NodeOp string_to_node_op(const std::string& str);

} // namespace recipe
} // namespace praxis
