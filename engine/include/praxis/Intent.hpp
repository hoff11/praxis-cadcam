#pragma once
#include <string>
#include <map>
#include <vector>
#include "../src/reasoning/ReasoningTypes.hpp"
#include "../src/recipe/RecipeTypes.hpp"

namespace praxis {

// Sprint 12: Previewable output metadata for contract compliance
struct PreviewableOutput {
    std::string filename;
    std::string type;           // step, stl, brep
    std::string semantic_type;  // Body, Product, Assembly
    bool previewable = false;
};

struct IntentRequest {
    std::string intent_name;
    std::map<std::string, std::string> params;
    std::string input_step_path;  // optional
    std::string output_dir;
    
    // Sprint 5 Phase 6: Cache control flags
    bool no_cache = false;       // Disable cache lookup and storage
    bool clear_cache = false;    // Clear cache directory before execution
    
    // Sprint 6 Phase 4: Cache management flags
    bool cache_stats = false;    // Show cache statistics and exit
    bool prune_cache = false;    // Prune old cache entries
    int max_age_days = 30;       // Max age for prune (default 30 days)
};

struct IntentResult {
    bool success = false;
    std::string report_version = "0.3";  // Sprint 3: reasoning layer added
    std::vector<std::string> artifacts;  // paths to output files
    std::string report_path;
    double confidence = 0.0;  // 0.0 to 1.0
    std::vector<std::string> errors;
    std::string error_code = "";  // Sprint 12: structured error taxonomy (InvalidParameter, IntentFailed, etc.)
    std::vector<std::string> warnings;
    std::vector<std::string> kernel_ops;  // for traceability
    std::map<std::string, std::string> metrics;  // shape_valid, num_faces, etc.
    double duration_ms = 0.0;  // execution time
    std::vector<reasoning::ReasoningNote> reasoning;  // Sprint 3: reasoning notes
    std::map<std::string, double> resolved_params;  // Sprint 3 EPIC 3: final resolved params
    std::map<std::string, double> derived_values;   // Sprint 3 EPIC 3: computed derived values
    std::vector<recipe::RecipeInclude> includes;    // Sprint 4: included recipes with overrides
    
    // Sprint 5 Phase 3: Plan hash
    std::string plan_hash = "";              // SHA256 hash of execution plan (empty if invalid)
    bool plan_hash_valid = false;            // Whether plan_hash is trustworthy
    std::string plan_hash_status = "";       // Reason if hash is invalid (empty if valid)
    
    // Sprint 5 Phase 4: Execution cache state
    struct CacheInfo {
        bool hit = false;                     // Whether this execution used cached artifacts
        std::string plan_hash = "";           // Hash key for cache lookup
        std::string reason = "";              // Cache miss reason (empty if hit)
        std::vector<std::string> reused_artifacts;  // Which artifacts were reused from cache
        int64_t cache_entry_age_seconds = 0;  // Age of cache entry (0 if miss)
    } cache;
    
    // Sprint 6 Phase 3: Op-level cache state
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
    
    struct OpCacheInfo {
        int executed_count = 0;
        int reused_count = 0;
        double cache_hit_rate = 0.0;
        std::string cache_dir;  // Absolute cache root for path normalization
        std::vector<NodeExecutionStatus> node_status;
    } op_cache;
    
    // Sprint 7: Semantic Interaction Contract (SIC)
    struct InteractionInfo {
        std::string contract_version = "1.0";
        std::string selector_contract_version = "1.0";
        
        // Engine-emitted selectables (authoritative, stable IDs)
        struct Selectable {
            std::string ref_type;   // BodyRef | FaceRef | EdgeRef | VertexRef (viewer schema)
            std::string stable_id;  // Stable identifier for this reference (identity only)
            std::string label;      // Human-readable label (optional)
            
            // Structured adjacency fields (v2.0 contract - identity/metadata split)
            // These provide topology context without embedding in stable_id
            struct FaceLocation {
                int body;
                int face;
            };
            std::vector<FaceLocation> faces;     // For EdgeRef: adjacent faces
            std::vector<int> vertices;           // For EdgeRef: endpoint vertices
            std::vector<int> edges;              // For VertexRef: incident edges
        };

        struct Selection {
            std::string mode;
            std::string target;
            std::string selector;
            std::vector<std::string> references_json;  // JSON-encoded Reference objects
            std::string timestamp;
        };
        
        struct Failure {
            std::string type;
            std::string message;
            std::string selector;
            std::string reference_json;
        };
        
        // Optional: list of selectable references for viewer
        std::vector<Selectable> selectables;

        std::vector<Selection> selections;
        std::vector<std::string> references_json;  // All references produced (JSON)
        std::vector<std::string> resolutions_json; // All resolution results (JSON), public schema
        std::vector<Failure> failures;
        std::string artifact_path;
        std::string inspection_json;              // Optional: serialized inspection block (JSON)
        std::string execution_timestamp;
        bool determinism_verified = false;
    } interaction;
    
    // Sprint 10 Task 3.1: Structured result summary
    struct Summary {
        std::string intent;                      // Human-readable intent description
        std::vector<std::string> produced;       // Semantic objects produced (ordered deterministically)
        int body_count = 0;
        int face_count = 0;
        int edge_count = 0;
        int artifact_count = 0;
        std::vector<praxis::PreviewableOutput> previewable_outputs;  // Sprint 12: structured artifact metadata
    } summary;
};

} // namespace praxis
