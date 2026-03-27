#pragma once
#include "../recipe/RecipeTypes.hpp"
#include <string>
#include <vector>
#include <map>
#include <variant>

// Forward declaration
namespace praxis { namespace cache { struct HashContext; } }

namespace praxis {
namespace plan {

// Single node in the execution graph (Sprint 6 Phase 1)
struct GraphNode {
    size_t node_index;                         // Stable index in execution order
    std::string node_id;                       // Original recipe node ID
    std::string op_type;                       // e.g., "make_box", "transform", "compound"
    std::map<std::string, std::variant<double, std::string>> args;  // Resolved args (numeric + strings)
    std::vector<size_t> deps;                  // Upstream node indices (sorted, unique)
    std::string output_name = "out";           // Phase 1: single output per node
    bool cacheable = false;                    // Phase 2: per-op cache eligibility
    
    // Phase 2: Op hash for content-addressable caching
    std::string op_hash;                       // SHA256 of canonical payload (empty until computed)
    
    // Phase 3 will add: was_reused
};

// Execution graph representing a DAG of operations (Sprint 6 Phase 1)
struct ExecutionGraph {
    std::vector<GraphNode> nodes;              // Nodes in topological order
    std::string output_node_id;                // ID of final output node
    
    // Deterministic debug serialization (for tests + report later)
    std::string to_canonical_json() const;
};

// Build execution graph from resolved plan (Sprint 6 Phase 1)
// Returns graph with deterministic node ordering and dependency structure
// Throws std::runtime_error if plan contains invalid dependencies
// Note: Requires original recipe to extract dependency structure from node args
ExecutionGraph build_graph_from_resolved_plan(
    const recipe::Recipe& recipe,
    const recipe::ResolvedPlan& plan
);

// Compute op_hash for all nodes in topological order (Sprint 6 Phase 2)
// Must be called after build_graph_from_resolved_plan
// Hashes computed in dependency order (all deps hashed before dependents)
// Throws std::runtime_error if topological order is violated
ExecutionGraph compute_hashes(ExecutionGraph graph, const cache::HashContext& ctx);

// Get expected output names for an op type (Sprint 6 Phase 4 EPIC 1)
// Returns list of output names that this op type produces
// Currently all ops produce single "out", but extensible for multi-output in future
std::vector<std::string> get_expected_outputs(const std::string& op_type);

} // namespace plan
} // namespace praxis
