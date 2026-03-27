#include "KernelOpHasher.hpp"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>

namespace praxis {
namespace cache {

// Build canonical JSON payload (deterministic, version-stamped)
std::string KernelOpHasher::canonical_payload(const plan::GraphNode& node,
                                              const plan::ExecutionGraph& graph,
                                              const HashContext& ctx) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(ctx.arg_precision);
    
    ss << "{\n";
    ss << "  \"schema\": " << ctx.schema_version << ",\n";
    ss << "  \"engine_version\": \"" << ctx.engine_version << "\",\n";
    ss << "  \"kernel_version\": \"" << ctx.kernel_version << "\",\n";
    ss << "  \"op_type\": \"" << node.op_type << "\",\n";
    
    // Args (already sorted by std::map)
    ss << "  \"args\": {";
    bool first_arg = true;
    for (const auto& kv : node.args) {
        if (!first_arg) ss << ", ";
        ss << "\"" << kv.first << "\": ";
        
        // Handle variant<double, string>
        if (std::holds_alternative<double>(kv.second)) {
            ss << std::get<double>(kv.second);
        } else {
            ss << "\"" << std::get<std::string>(kv.second) << "\"";
        }
        
        first_arg = false;
    }
    ss << "},\n";
    
    // Deps (ordered by graph deps, which are already sorted)
    ss << "  \"deps\": [";
    for (size_t i = 0; i < node.deps.size(); ++i) {
        if (i > 0) ss << ", ";
        
        size_t dep_idx = node.deps[i];
        const std::string& dep_hash = graph.nodes[dep_idx].op_hash;
        
        if (dep_hash.empty()) {
            throw std::runtime_error("Hash computation error: dependency node " + 
                                   std::to_string(dep_idx) + " has no op_hash (topological order violated)");
        }
        
        ss << "\"" << dep_hash << "\"";
    }
    ss << "],\n";
    
    // Outputs (single output contract for Phase 2)
    ss << "  \"outputs\": [\"" << ctx.output_contract << "\"]\n";
    ss << "}";
    
    return ss.str();
}

// Compute SHA256 hex digest
std::string KernelOpHasher::sha256_hex(const std::string& bytes) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(bytes.c_str()), bytes.size(), hash);
    
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::setw(2) << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

// Compute op_hash for a node
std::string KernelOpHasher::hash_node(const plan::GraphNode& node,
                                     const plan::ExecutionGraph& graph,
                                     const HashContext& ctx) {
    std::string payload = canonical_payload(node, graph, ctx);
    return sha256_hex(payload);
}

} // namespace cache
} // namespace praxis
