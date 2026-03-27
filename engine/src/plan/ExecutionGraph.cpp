#include "ExecutionGraph.hpp"
#include "../cache/KernelOpHasher.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace praxis {
namespace plan {

// Helper: Get input keys for an operation type (Sprint 6 Phase 1 dependency extraction)
static std::vector<std::string> input_keys_for_op(const std::string& op_type) {
    if (op_type == "make_box") {
        return {};  // No dependencies
    } else if (op_type == "transform") {
        return {"input"};  // Depends on one shape
    } else if (op_type == "compound") {
        // Compound takes variable number of inputs
        // Will be handled specially in dependency extraction
        return {};
    }
    // Unknown op type - no dependencies assumed
    return {};
}

// Helper: Convert NodeOp enum to string
static std::string op_type_to_string(recipe::NodeOp op) {
    switch (op) {
        case recipe::NodeOp::MakeBox: return "make_box";
        case recipe::NodeOp::Transform: return "transform";
        case recipe::NodeOp::Compound: return "compound";
        default: return "unknown";
    }
}

// Helper: Extract dependency node IDs from operation args
static std::vector<std::string> extract_dep_node_ids(
    const recipe::NodeOp& op,
    const recipe::NodeArgs& args
) {
    std::vector<std::string> deps;
    
    if (op == recipe::NodeOp::Transform) {
        if (auto* transform_args = std::get_if<recipe::TransformArgs>(&args)) {
            deps.push_back(transform_args->input);
        }
    } else if (op == recipe::NodeOp::Compound) {
        if (auto* compound_args = std::get_if<recipe::CompoundArgs>(&args)) {
            deps = compound_args->node_refs;
        }
    }
    // MakeBox has no dependencies
    
    return deps;
}

ExecutionGraph build_graph_from_resolved_plan(
    const recipe::Recipe& recipe,
    const recipe::ResolvedPlan& plan
) {
    ExecutionGraph graph;
    graph.output_node_id = plan.output_node;
    
    // Build node_id -> producer_index mapping
    std::map<std::string, size_t> node_id_to_index;
    for (size_t i = 0; i < plan.operations.size(); ++i) {
        node_id_to_index[plan.operations[i].node_id] = i;
    }
    
    // Build graph nodes
    for (size_t i = 0; i < plan.operations.size(); ++i) {
        const auto& resolved_op = plan.operations[i];
        
        // Find corresponding recipe node to extract dependency structure
        const recipe::RecipeNode* recipe_node = nullptr;
        for (const auto& node : recipe.nodes) {
            if (node.id == resolved_op.node_id) {
                recipe_node = &node;
                break;
            }
        }
        
        if (!recipe_node) {
            throw std::runtime_error("Graph build error: resolved operation '" + 
                                   resolved_op.node_id + "' not found in recipe");
        }
        
        GraphNode node;
        node.node_index = i;
        node.node_id = resolved_op.node_id;
        node.op_type = op_type_to_string(resolved_op.op);
        
        // Convert resolved_args (map<string, double>) to node.args (map<string, variant>)
        for (const auto& kv : resolved_op.resolved_args) {
            node.args[kv.first] = kv.second;  // Implicit conversion double -> variant<double, string>
        }
        
        node.output_name = "out";  // Phase 1: all nodes produce "out"
        node.cacheable = false;    // Phase 1: placeholder for Phase 2
        
        // Extract dependencies from recipe node args
        std::vector<std::string> dep_node_ids = extract_dep_node_ids(
            recipe_node->op, 
            recipe_node->args
        );
        
        // Convert node IDs to indices
        std::set<size_t> dep_indices_set;  // Use set for dedup + sorting
        for (const auto& dep_id : dep_node_ids) {
            auto it = node_id_to_index.find(dep_id);
            if (it == node_id_to_index.end()) {
                throw std::runtime_error("Graph build error: node '" + resolved_op.node_id + 
                                       "' depends on non-existent node '" + dep_id + "'");
            }
            
            size_t dep_index = it->second;
            if (dep_index >= i) {
                throw std::runtime_error("Graph build error: node '" + resolved_op.node_id + 
                                       "' has forward reference to '" + dep_id + "'");
            }
            
            dep_indices_set.insert(dep_index);
        }
        
        // Store deps in sorted order (deterministic)
        node.deps.assign(dep_indices_set.begin(), dep_indices_set.end());
        
        graph.nodes.push_back(node);
    }
    
    return graph;
}

// Deterministic JSON serialization for testing (Sprint 6 Phase 1)
std::string ExecutionGraph::to_canonical_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    
    ss << "{\n";
    ss << "  \"nodes\": [\n";
    
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        ss << "    {\n";
        ss << "      \"node_index\": " << node.node_index << ",\n";
        ss << "      \"node_id\": \"" << node.node_id << "\",\n";
        ss << "      \"op_type\": \"" << node.op_type << "\",\n";
        
        // Args in sorted order
        ss << "      \"args\": {";
        bool first_arg = true;
        std::vector<std::string> arg_keys;
        for (const auto& kv : node.args) {
            arg_keys.push_back(kv.first);
        }
        std::sort(arg_keys.begin(), arg_keys.end());
        
        for (const auto& key : arg_keys) {
            if (!first_arg) ss << ", ";
            ss << "\"" << key << "\": ";
            
            // Handle variant<double, string>
            const auto& value = node.args.at(key);
            if (std::holds_alternative<double>(value)) {
                ss << std::get<double>(value);
            } else {
                ss << "\"" << std::get<std::string>(value) << "\"";
            }
            
            first_arg = false;
        }
        ss << "},\n";
        
        // Deps (sorted)
        ss << "      \"deps\": [";
        for (size_t j = 0; j < node.deps.size(); ++j) {
            if (j > 0) ss << ", ";
            ss << node.deps[j];
        }
        ss << "],\n";
        
        ss << "      \"output_name\": \"" << node.output_name << "\",\n";
        ss << "      \"cacheable\": " << (node.cacheable ? "true" : "false");
        
        // Include op_hash if computed (Phase 2)
        if (!node.op_hash.empty()) {
            ss << ",\n";
            ss << "      \"op_hash\": \"" << node.op_hash << "\"";
        }
        
        ss << "\n";
        ss << "    }";
        if (i < nodes.size() - 1) ss << ",";
        ss << "\n";
    }
    
    ss << "  ],\n";
    ss << "  \"output_node_id\": \"" << output_node_id << "\"\n";
    ss << "}";
    
    return ss.str();
}

// Compute op_hash for all nodes in topological order (Sprint 6 Phase 2)
ExecutionGraph compute_hashes(ExecutionGraph graph, const cache::HashContext& ctx) {
    // Nodes are already in topological order (enforced by build_graph_from_resolved_plan)
    // Just compute hashes in order; each node's deps will already have hashes computed
    
    for (auto& node : graph.nodes) {
        // Validate topological order (belt-and-suspenders check)
        for (size_t dep_idx : node.deps) {
            if (dep_idx >= node.node_index) {
                throw std::runtime_error("Hash computation error: node " + node.node_id + 
                                       " has forward dependency (topological order violated)");
            }
            if (graph.nodes[dep_idx].op_hash.empty()) {
                throw std::runtime_error("Hash computation error: dependency node " + 
                                       std::to_string(dep_idx) + " has no op_hash");
            }
        }
        
        // Compute hash
        node.op_hash = cache::KernelOpHasher::hash_node(node, graph, ctx);
    }
    
    return graph;
}

// Get expected output names for an op type (Sprint 6 Phase 4 EPIC 1)
std::vector<std::string> get_expected_outputs(const std::string& op_type) {
    // Phase 4: All ops currently produce single "out"
    // Future: ops can declare multiple outputs
    // e.g., "split" might produce ["upper", "lower"]
    // e.g., "extrude" might produce ["out", "debug_profile"]
    
    // For now, all ops produce exactly one output named "out"
    return {"out"};
    
    // Future expansion example:
    // if (op_type == "split") {
    //     return {"upper", "lower"};
    // } else if (op_type == "extrude_with_debug") {
    //     return {"out", "debug_profile"};
    // }
    // return {"out"};  // default
}

} // namespace plan
} // namespace praxis
