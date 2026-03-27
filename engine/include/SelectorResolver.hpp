/**
 * SelectorResolver.hpp
 * 
 * Phase C.5: Selector Resolution (AST → Stable IDs)
 * 
 * Purpose: Resolve parsed selector AST against artifact topology
 * Contract: Pure query - no mutation, deterministic results
 * 
 * Input:  Selector AST (from Phase C.4 parser) + Artifact
 * Output: Vector of stable_ids or typed resolution error
 * 
 * Stable ID Format (v0.5.0):
 * - Bodies:   "body_{index}_{signature16}"
 * - Faces:    "face_b{body}_{index}_{signature16}"
 * - Edges:    "edge_b{body}_{index}_{signature16}"
 * - Vertices: "vertex_{index}_{signature16}"
 * 
 * Notes:
 * - Faces/Edges include body_index to prevent collisions across bodies
 * - Vertices use global index (deduplicated by inspector)
 * - Signature prefix length = 16 chars (defined in .cpp)
 */

#pragma once

#include "Selector.hpp"
#include "Inspection.hpp"
#include "SelectorException.hpp"
#include <vector>
#include <string>
#include <optional>
#include <any>
#include <variant>

namespace praxis {
namespace selector {

// ============================================================================
// Resolution Error Types
// ============================================================================

enum class ResolveErrorKind {
    NoMatches,          // Selector valid but no topology matches
    PropertyNotFound,   // Filter references unknown property
    TypeMismatch,       // Filter value incompatible with property type
    InspectorError      // Topology query failed
};

struct ResolveError {
    ResolveErrorKind kind;
    std::string message;
    std::string selector_canonical;  // For debugging
};

// ============================================================================
// Resolution Result
// ============================================================================

struct ResolveResult {
    std::vector<std::string> stable_ids;   // Resolved topology IDs
    std::optional<ResolveError> error;
    int match_count = 0;
    std::string selector_canonical;
    
    bool success() const { return !error.has_value(); }
};

// ============================================================================
// Selector Resolver
// ============================================================================

/**
 * SelectorResolver - Query topology using parsed selector AST
 * 
 * Phase C.5 implementation: Candidate-based filtering with cached properties
 * 
 * Correctness fix (v0.5.0):
 * - Builds candidate table once per resolve() with direct property access
 * - No index parsing from stable_ids (multi-body safe)
 * - No repeated enumeration (performance improvement)
 * 
 * Usage:
 *   Inspection inspector(artifact);
 *   SelectorResolver resolver(inspector);
 *   auto result = resolver.resolve(parsed_selector);
 *   
 *   if (result.success()) {
 *       for (const auto& id : result.stable_ids) {
 *           // Use stable_id...
 *       }
 *   }
 */
class SelectorResolver {
public:
    explicit SelectorResolver(inspection::Inspector& inspector);
    
    /**
     * Resolve selector AST to stable topology IDs
     * 
     * Returns all matching entities or error if:
     * - No matches found
     * - Invalid property reference (throws PropertyNotFoundException)
     * - Type mismatch in filter (throws TypeMismatchException)
     */
    ResolveResult resolve(const Selector& selector);
    
private:
    inspection::Inspector& inspector_;
    
    // Candidate structure: holds stable_id + direct property access
    struct Candidate {
        std::string stable_id;
        std::variant<
            inspection::BodyProperties,
            inspection::FaceProperties,
            inspection::EdgeProperties,
            inspection::VertexProperties
        > properties;
    };
    
    // Build candidate table once per resolve()
    std::vector<Candidate> build_candidates(SelectorTarget target);
    
    // Filter execution using candidates (no re-enumeration)
    bool matches_filters(const Candidate& candidate,
                        SelectorTarget target,
                        const std::vector<Filter>& filters);
    
    std::optional<std::any> get_property(const Candidate& candidate,
                                         SelectorTarget target,
                                         const std::string& field);
    
    bool compare_values(const std::any& prop_value,
                       FilterOp op,
                       const Literal& filter_value);
    
    // Type coercion helpers (for std::any → primitive conversions)
    std::optional<double> to_double(const std::any& value);
    std::optional<int64_t> to_int(const std::any& value);
    std::optional<std::string> any_to_string(const std::any& value);
};

// ============================================================================
// Utilities
// ============================================================================

std::string to_string(ResolveErrorKind kind);

}  // namespace selector
}  // namespace praxis
