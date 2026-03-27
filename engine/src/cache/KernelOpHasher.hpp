#pragma once
#include "HashContext.hpp"
#include "../plan/ExecutionGraph.hpp"
#include <string>

namespace praxis {
namespace cache {

// Computes deterministic op hashes for execution graph nodes (Sprint 6 Phase 2)
struct KernelOpHasher {
    // Build canonical JSON payload for hashing
    static std::string canonical_payload(const plan::GraphNode& node,
                                        const plan::ExecutionGraph& graph,
                                        const HashContext& ctx);
    
    // Compute SHA256 hex digest
    static std::string sha256_hex(const std::string& bytes);
    
    // Compute op_hash for a node (returns hex SHA256)
    static std::string hash_node(const plan::GraphNode& node,
                                 const plan::ExecutionGraph& graph,
                                 const HashContext& ctx);
};

} // namespace cache
} // namespace praxis
