#include "praxis/Report.hpp"
#include "praxis/CanonicalFormat.hpp"
#include "openssl/sha.h"
#include <openssl/evp.h>
#include "InteractionEmit.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <filesystem>

namespace praxis {

bool Report::writeReport(const IntentRequest& request,
                        const IntentResult& result,
                        const std::string& outputPath) {
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "{\n";
    file << "  \"version\": \"praxis-cad/" << PRAXIS_ENGINE_VERSION << "\",\n";
    file << "  \"report_version\": \"" << result.report_version << "\",\n";
    file << "  \"intent\": \"" << canonical::escape_json(request.intent_name) << "\",\n";
    file << "  \"success\": " << (result.success ? "true" : "false") << ",\n";
    file << "  \"duration_ms\": " << canonical::format_float(result.duration_ms) << ",\n";
    
    // Sprint 10 Task 3.1 & Sprint 11 Task 11.4: Result summary
    file << "  \"summary\": {\n";
    file << "    \"intent\": \"" << canonical::escape_json(result.summary.intent) << "\",\n";
    file << "    \"produced\": [";
    for (size_t i = 0; i < result.summary.produced.size(); ++i) {
        if (i > 0) file << ", ";
        file << "\"" << canonical::escape_json(result.summary.produced[i]) << "\"";
    }
    file << "],\n";
    file << "    \"body_count\": " << result.summary.body_count << ",\n";
    file << "    \"face_count\": " << result.summary.face_count << ",\n";
    file << "    \"edge_count\": " << result.summary.edge_count << ",\n";
    file << "    \"artifact_count\": " << result.summary.artifact_count << ",\n";
    file << "    \"previewable_outputs\": [";
    for (size_t i = 0; i < result.summary.previewable_outputs.size(); ++i) {
        if (i > 0) file << ", ";
        const auto& output = result.summary.previewable_outputs[i];
        file << "\n      {\n";
        file << "        \"filename\": \"" << canonical::escape_json(output.filename) << "\",\n";
        file << "        \"type\": \"" << canonical::escape_json(output.type) << "\",\n";
        file << "        \"semantic_type\": \"" << canonical::escape_json(output.semantic_type) << "\",\n";
        file << "        \"previewable\": " << (output.previewable ? "true" : "false") << "\n";
        file << "      }";
    }
    if (!result.summary.previewable_outputs.empty()) {
        file << "\n    ";
    }
    file << "]\n";
    file << "  },\n";
    
    // Input params (CLI)
    file << "  \"inputs\": {";
    bool first = true;
    for (const auto& [key, value] : request.params) {
        if (!first) file << ",";
        file << "\n    \"" << canonical::escape_json(key) << "\": \"" << canonical::escape_json(value) << "\"";
        first = false;
    }
    if (!request.input_step_path.empty()) {
        if (!first) file << ",";
        file << "\n    \"input_step_path\": \"" << canonical::escape_json(request.input_step_path) << "\"";
    }
    file << "\n  },\n";
    
    // Resolved inputs (Sprint 3 EPIC 3)
    file << "  \"resolved_inputs\": {\n";
    file << "    \"params\": {";
    first = true;
    for (const auto& [key, value] : result.resolved_params) {
        if (!first) file << ",";
        file << "\n      \"" << canonical::escape_json(key) << "\": " << canonical::format_float(value);
        first = false;
    }
    file << "\n    },\n";
    file << "    \"derived\": {";
    first = true;
    for (const auto& [key, value] : result.derived_values) {
        if (!first) file << ",";
        file << "\n      \"" << canonical::escape_json(key) << "\": " << canonical::format_float(value);
        first = false;
    }
    file << "\n    }\n";
    file << "  },\n";
    
    // Artifacts
    file << "  \"artifacts\": [";
    for (size_t i = 0; i < result.artifacts.size(); ++i) {
        if (i > 0) file << ", ";
        file << "\"" << canonical::escape_json(result.artifacts[i]) << "\"";
    }
    file << "],\n";
    
    // Kernel ops (for traceability)
    file << "  \"kernel_ops\": [";
    for (size_t i = 0; i < result.kernel_ops.size(); ++i) {
        if (i > 0) file << ", ";
        file << "\"" << canonical::escape_json(result.kernel_ops[i]) << "\"";
    }
    file << "],\n";
    
    // Metrics
    file << "  \"metrics\": {";
    first = true;
    for (const auto& [key, value] : result.metrics) {
        if (!first) file << ",";
        file << "\n    \"" << canonical::escape_json(key) << "\": \"" << canonical::escape_json(value) << "\"";
        first = false;
    }
    file << "\n  },\n";
    
    // Confidence
    file << "  \"confidence\": " << canonical::format_float(result.confidence) << ",\n";
    
    // Warnings
    file << "  \"warnings\": [";
    for (size_t i = 0; i < result.warnings.size(); ++i) {
        if (i > 0) file << ", ";
        file << "\"" << canonical::escape_json(result.warnings[i]) << "\"";
    }
    file << "],\n";
    
    // Errors
    file << "  \"errors\": [";
    for (size_t i = 0; i < result.errors.size(); ++i) {
        if (i > 0) file << ", ";
        file << "\"" << canonical::escape_json(result.errors[i]) << "\"";
    }
    file << "],\n";
    
    // Includes (Sprint 4 provenance)
    file << "  \"includes\": [\n";
    for (size_t i = 0; i < result.includes.size(); ++i) {
        if (i > 0) file << ",\n";
        const auto& inc = result.includes[i];
        file << "    {\n";
        file << "      \"recipe_path\": \"" << canonical::escape_json(inc.recipe_path) << "\",\n";
        file << "      \"param_overrides\": {";
        bool first_param = true;
        for (const auto& [key, value] : inc.param_overrides) {
            if (!first_param) file << ",";
            file << "\n        \"" << canonical::escape_json(key) << "\": " << canonical::format_float(value);
            first_param = false;
        }
        file << "\n      }\n";
        file << "    }";
    }
    file << "\n  ],\n";
    
    // Plan hash (Sprint 5 Phase 3)
    file << "  \"plan_hash\": {\n";
    file << "    \"hash\": \"" << canonical::escape_json(result.plan_hash) << "\",\n";
    file << "    \"valid\": " << (result.plan_hash_valid ? "true" : "false");
    if (!result.plan_hash_status.empty()) {
        file << ",\n";
        file << "    \"status\": \"" << canonical::escape_json(result.plan_hash_status) << "\"";
    }
    file << "\n  },\n";
    
    // Cache info (Sprint 5 Phase 4)
    file << "  \"cache\": {\n";
    file << "    \"hit\": " << (result.cache.hit ? "true" : "false") << ",\n";
    file << "    \"plan_hash\": \"" << canonical::escape_json(result.cache.plan_hash) << "\"";
    if (!result.cache.reason.empty()) {
        file << ",\n";
        file << "    \"reason\": \"" << canonical::escape_json(result.cache.reason) << "\"";
    }
    if (!result.cache.reused_artifacts.empty()) {
        file << ",\n";
        file << "    \"reused_artifacts\": [";
        for (size_t i = 0; i < result.cache.reused_artifacts.size(); ++i) {
            if (i > 0) file << ", ";
            file << "\"" << canonical::escape_json(result.cache.reused_artifacts[i]) << "\"";
        }
        file << "]";
    }
    if (result.cache.cache_entry_age_seconds > 0) {
        file << ",\n";
        file << "    \"cache_entry_age_seconds\": " << result.cache.cache_entry_age_seconds;
    }
    file << "\n  },\n";
    
    // Op-level cache info (Sprint 6 Phase 3 Step 4)
    file << "  \"op_cache\": {\n";
    file << "    \"executed_count\": " << result.op_cache.executed_count << ",\n";
    file << "    \"reused_count\": " << result.op_cache.reused_count << ",\n";
    file << "    \"cache_hit_rate\": " << canonical::format_float(result.op_cache.cache_hit_rate) << ",\n";
    file << "    \"cache_dir\": \"" << canonical::escape_json(result.op_cache.cache_dir) << "\",\n";
    file << "    \"node_status\": [\n";
    for (size_t i = 0; i < result.op_cache.node_status.size(); ++i) {
        if (i > 0) file << ",\n";
        const auto& node = result.op_cache.node_status[i];
        
        // Make artifact_path relative to cache_dir for machine-stable reports
        // Use lexically_relative for safe path manipulation
        std::string display_path = node.artifact_path;
        if (!result.op_cache.cache_dir.empty() && !display_path.empty()) {
            try {
                std::filesystem::path abs_artifact(display_path);
                std::filesystem::path abs_cache(result.op_cache.cache_dir);
                auto rel = abs_artifact.lexically_relative(abs_cache);
                if (!rel.empty() && rel.string() != ".") {
                    display_path = rel.string();
                }
            } catch (...) {
                // If path operations fail, keep absolute path
            }
        }
        
        file << "      {\n";
        file << "        \"node_id\": \"" << canonical::escape_json(node.node_id) << "\",\n";
        file << "        \"node_index\": " << node.node_index << ",\n";
        file << "        \"op_type\": \"" << canonical::escape_json(node.op_type) << "\",\n";
        file << "        \"op_hash\": \"" << canonical::escape_json(node.op_hash) << "\",\n";
        file << "        \"status\": \"" << canonical::escape_json(node.status) << "\",\n";
        file << "        \"cache_hit\": " << (node.cache_hit ? "true" : "false") << ",\n";
        file << "        \"artifact_path\": \"" << canonical::escape_json(display_path) << "\"\n";
        file << "      }";
    }
    file << "\n    ]\n";
    file << "  },\n";
    
    // Reasoning notes (Sprint 3)
    file << "  \"reasoning\": [\n";
    for (size_t i = 0; i < result.reasoning.size(); ++i) {
        if (i > 0) file << ",\n";
        const auto& note = result.reasoning[i];
        file << "    {\n";
        file << "      \"level\": \"" << reasoning::level_to_string(note.level) << "\",\n";
        file << "      \"rule_id\": \"" << canonical::escape_json(note.rule_id) << "\",\n";
        file << "      \"code\": \"" << canonical::escape_json(note.code) << "\",\n";
        file << "      \"message\": \"" << canonical::escape_json(note.message) << "\",\n";
        file << "      \"source\": \"" << canonical::escape_json(note.source) << "\",\n";
        file << "      \"actual_value\": " << canonical::format_float(note.actual_value) << ",\n";
        file << "      \"threshold\": " << canonical::format_float(note.threshold) << ",\n";
        file << "      \"units\": \"" << canonical::escape_json(note.units) << "\"\n";
        file << "    }";
    }
    file << "\n  ],\n";
    
    // Interaction (Sprint 7: Semantic Interaction Contract)
    // Ensure deterministic ordering of interaction arrays before emission
    IntentResult orderedResult = result;
    finalize_interaction_ordering(orderedResult);
    file << "  \"interaction\": {\n";
    file << "    \"contract_version\": \"" << canonical::escape_json(orderedResult.interaction.contract_version) << "\",\n";
    file << "    \"selector_contract_version\": \"" << canonical::escape_json(orderedResult.interaction.selector_contract_version) << "\",\n";
    if (!orderedResult.interaction.inspection_json.empty()) {
        file << "    \"inspection\": " << orderedResult.interaction.inspection_json << ",\n";
    }
    // Optional selectables for viewer
    if (!orderedResult.interaction.selectables.empty()) {
        file << "    \"selectables\": [";
        for (size_t i = 0; i < orderedResult.interaction.selectables.size(); ++i) {
            if (i > 0) file << ",";
            const auto& s = orderedResult.interaction.selectables[i];
            file << "\n      {\n";
            file << "        \"ref_type\": \"" << canonical::escape_json(s.ref_type) << "\",\n";
            file << "        \"stable_id\": \"" << canonical::escape_json(s.stable_id) << "\",\n";
            file << "        \"label\": \"" << canonical::escape_json(s.label) << "\"";
            
            // Emit structured adjacency fields if present (v2.0 contract)
            if (!s.faces.empty()) {
                file << ",\n        \"faces\": [";
                for (size_t j = 0; j < s.faces.size(); ++j) {
                    if (j > 0) file << ", ";
                    file << "{\"body\": " << s.faces[j].body << ", \"face\": " << s.faces[j].face << "}";
                }
                file << "]";
            }
            if (!s.vertices.empty()) {
                file << ",\n        \"vertices\": [";
                for (size_t j = 0; j < s.vertices.size(); ++j) {
                    if (j > 0) file << ", ";
                    file << s.vertices[j];
                }
                file << "]";
            }
            if (!s.edges.empty()) {
                file << ",\n        \"edges\": [";
                for (size_t j = 0; j < s.edges.size(); ++j) {
                    if (j > 0) file << ", ";
                    file << s.edges[j];
                }
                file << "]";
            }
            
            file << "\n      }";
        }
        file << "\n    ],\n";
    }
    file << "    \"selections\": [";
    for (size_t i = 0; i < orderedResult.interaction.selections.size(); ++i) {
        if (i > 0) file << ",";
        const auto& sel = orderedResult.interaction.selections[i];
        file << "\n      {\n";
        file << "        \"mode\": \"" << canonical::escape_json(sel.mode) << "\",\n";
        file << "        \"target\": \"" << canonical::escape_json(sel.target) << "\",\n";
        file << "        \"selector\": \"" << canonical::escape_json(sel.selector) << "\",\n";
        file << "        \"references\": [";
        for (size_t j = 0; j < sel.references_json.size(); ++j) {
            if (j > 0) file << ", ";
            const std::string& s = sel.references_json[j];
            if (!s.empty() && (s.front() == '{' || s.front() == '[')) {
                file << s;  // JSON-like
            } else {
                file << "\"" << canonical::escape_json(s) << "\""; // Fallback safety
            }
        }
        file << "]\n";  // No trailing comma, timestamp key omitted entirely
        file << "      }";
    }
    file << "\n    ],\n";
    file << "    \"references\": [";
    for (size_t i = 0; i < orderedResult.interaction.references_json.size(); ++i) {
        if (i > 0) file << ", ";
        const std::string& s = orderedResult.interaction.references_json[i];
        if (!s.empty() && (s.front() == '{' || s.front() == '[')) {
            file << s;
        } else {
            file << "\"" << canonical::escape_json(s) << "\"";
        }
    }
    file << "],\n";
    // Resolutions (Sprint 8 public schema)
    file << "    \"resolutions\": [";
    for (size_t i = 0; i < orderedResult.interaction.resolutions_json.size(); ++i) {
        if (i > 0) file << ", ";
        const std::string& s = orderedResult.interaction.resolutions_json[i];
        if (!s.empty() && (s.front() == '{' || s.front() == '[')) {
            file << s;
        } else {
            file << "\"" << canonical::escape_json(s) << "\"";
        }
    }
    file << "],\n";
    file << "    \"failures\": [";
    for (size_t i = 0; i < orderedResult.interaction.failures.size(); ++i) {
        if (i > 0) file << ",";
        const auto& fail = orderedResult.interaction.failures[i];
        file << "\n      {\n";
        file << "        \"type\": \"" << canonical::escape_json(fail.type) << "\",\n";
        file << "        \"message\": \"" << canonical::escape_json(fail.message) << "\"";
        if (!fail.selector.empty()) {
            file << ",\n";
            file << "        \"selector\": \"" << canonical::escape_json(fail.selector) << "\"";
        }
        if (!fail.reference_json.empty()) {
            file << ",\n";
            file << "        \"reference\": " << fail.reference_json;  // Already JSON
        }
        file << "\n      }";
    }
    file << "\n    ]";
    if (!result.interaction.artifact_path.empty() || 
        !result.interaction.execution_timestamp.empty() || 
        result.interaction.determinism_verified) {
        file << ",\n";
        file << "    \"metadata\": {\n";
        if (!result.interaction.artifact_path.empty()) {
            file << "      \"artifact_path\": \"" << canonical::escape_json(result.interaction.artifact_path) << "\"";
            if (!result.interaction.execution_timestamp.empty() || result.interaction.determinism_verified) {
                file << ",\n";
            } else {
                file << "\n";
            }
        }
        if (!result.interaction.execution_timestamp.empty()) {
            file << "      \"execution_timestamp\": \"" << canonical::escape_json(result.interaction.execution_timestamp) << "\"";
            if (result.interaction.determinism_verified) {
                file << ",\n";
            } else {
                file << "\n";
            }
        }
        if (result.interaction.determinism_verified) {
            file << "      \"determinism_verified\": true\n";
        }
        file << "    }";
    }
    // Provenance per Sprint 8
    {
        std::string artifact_hash;
        if (!result.interaction.artifact_path.empty()) {
            // Compute SHA-256 using EVP (OpenSSL 3-compatible)
            std::ifstream ifs(result.interaction.artifact_path, std::ios::binary);
            if (ifs) {
                EVP_MD_CTX* ctx = EVP_MD_CTX_new();
                if (ctx && EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1) {
                    std::vector<char> buffer(1 << 16);
                    while (ifs) {
                        ifs.read(buffer.data(), buffer.size());
                        std::streamsize got = ifs.gcount();
                        if (got > 0) {
                            EVP_DigestUpdate(ctx, buffer.data(), static_cast<size_t>(got));
                        }
                    }
                    unsigned char digest[EVP_MAX_MD_SIZE];
                    unsigned int digest_len = 0;
                    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) == 1) {
                        static const char* hex = "0123456789abcdef";
                        artifact_hash.resize(digest_len * 2);
                        for (unsigned int i = 0; i < digest_len; ++i) {
                            artifact_hash[2*i] = hex[(digest[i] >> 4) & 0xF];
                            artifact_hash[2*i+1] = hex[digest[i] & 0xF];
                        }
                    }
                }
                if (ctx) {
                    EVP_MD_CTX_free(ctx);
                }
            }
        }
        std::string selector_value;
        if (!result.interaction.selections.empty()) {
            selector_value = result.interaction.selections.back().selector;
        }
        if (!artifact_hash.empty() || !selector_value.empty() || !orderedResult.interaction.execution_timestamp.empty()) {
            file << ",\n";
            file << "    \"provenance\": {\n";
            bool wrote_any = false;
            if (!artifact_hash.empty()) {
                file << "      \"artifact_hash\": \"" << canonical::escape_json(artifact_hash) << "\"";
                wrote_any = true;
            }
            if (!selector_value.empty()) {
                if (wrote_any) file << ",\n"; else wrote_any = true;
                file << "      \"selector\": \"" << canonical::escape_json(selector_value) << "\"";
            }
            if (wrote_any) file << ",\n";
            file << "      \"contract_version\": \"" << canonical::escape_json(orderedResult.interaction.contract_version) << "\"";
            if (!orderedResult.interaction.execution_timestamp.empty()) {
                file << ",\n";
                file << "      \"timestamp\": \"" << canonical::escape_json(orderedResult.interaction.execution_timestamp) << "\"";
            }
            file << "\n    }";
        }
    }
    file << "\n  }\n";
    
    file << "}\n";
    
    file.close();
    return true;
}


} // namespace praxis
