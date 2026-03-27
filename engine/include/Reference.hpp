/**
 * Reference.hpp
 * 
 * Reference encoding and resolution for Praxis Semantic Interaction Contract (SIC)
 * Sprint 7 - Epic 2: Reference Encoding + Resolution
 * 
 * Purpose: Stable, serializable references that survive regeneration
 * Contract: Per reference_model.md - deterministic resolution with explicit drift detection
 * 
 * Reference lifecycle: Selection (ephemeral) → Reference (stable) → Resolution (validated)
 */

#pragma once

#include "Inspection.hpp"
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>

namespace praxis {
namespace reference {

// Contract version
constexpr const char* CONTRACT_VERSION = "1.0";

// ============================================================================
// Reference Types (Canonical JSON Encoding)
// ============================================================================

using JsonValue = std::variant<int, double, std::string, std::vector<double>>;

struct BodyRef {
    std::string ref_type = "BodyRef";
    std::string contract_version = CONTRACT_VERSION;
    int index;
    struct {
        double volume;
        inspection::Vector3 centroid;
        inspection::BoundingBox bbox;
    } signature;
    
    std::string to_json() const;
    static std::optional<BodyRef> from_json(const std::string& json);
};

struct FaceRef {
    std::string ref_type = "FaceRef";
    std::string contract_version = CONTRACT_VERSION;
    BodyRef parent;
    int index;
    struct {
        inspection::SurfaceType surface_type;
        double area;
        inspection::Vector3 centroid;
        std::optional<inspection::Vector3> normal;
        inspection::BoundingBox bbox;
    } signature;
    
    std::string to_json() const;
    static std::optional<FaceRef> from_json(const std::string& json);
};

struct EdgeRef {
    std::string ref_type = "EdgeRef";
    std::string contract_version = CONTRACT_VERSION;
    std::vector<FaceRef> parent_faces;
    int index;
    struct {
        inspection::EdgeType edge_type;
        std::optional<double> length;
        std::optional<double> radius;
        inspection::Vector3 midpoint;
        inspection::BoundingBox bbox;
    } signature;
    
    std::string to_json() const;
    static std::optional<EdgeRef> from_json(const std::string& json);
};

// Union type for generic reference handling
using Reference = std::variant<BodyRef, FaceRef, EdgeRef>;

// ============================================================================
// Resolution Result
// ============================================================================

enum class ResolutionStatus {
    Success,         // Reference resolved successfully
    Missing,         // Geometry removed
    Drifted,         // Signature mismatch beyond tolerance
    Ambiguous,       // Multiple matches
    ContractMismatch // contract_version mismatch
};

struct ResolutionResult {
    ResolutionStatus status;
    std::optional<Reference> resolved;
    std::string message;
    
    // For debugging and provenance
    std::map<std::string, double> signature_deltas;
};

// ============================================================================
// Reference Resolver
// ============================================================================

/**
 * ReferenceResolver - Resolve references against artifacts with drift detection
 * 
 * Contract guarantees:
 * - Deterministic: same artifact + reference → same resolution result
 * - Loud failures: drift/ambiguity are explicit, never silent
 * - Version enforcement: contract_version mismatch is an error
 */
class ReferenceResolver {
public:
    ReferenceResolver(inspection::Inspector* inspector);
    
    /**
     * Set tolerance for signature matching (default: 1e-6)
     */
    void set_tolerance(double tolerance);
    
    /**
     * Resolve a BodyRef against loaded artifact
     */
    ResolutionResult resolve(const BodyRef& ref);
    
    /**
     * Resolve a FaceRef against loaded artifact
     */
    ResolutionResult resolve(const FaceRef& ref);
    
    /**
     * Resolve an EdgeRef against loaded artifact
     */
    ResolutionResult resolve(const EdgeRef& ref);
    
    /**
     * Resolve generic reference variant
     */
    ResolutionResult resolve(const Reference& ref);
    
    /**
     * Resolve multiple references
     * Returns results in same order as input
     */
    std::vector<ResolutionResult> resolve_all(const std::vector<Reference>& refs);
    
private:
    inspection::Inspector* inspector_;
    double tolerance_ = 1e-6;
    
    // Signature matching helpers
    bool matches_within_tolerance(double a, double b) const;
    bool vector_matches(const inspection::Vector3& a, const inspection::Vector3& b) const;
    bool bbox_matches(const inspection::BoundingBox& a, const inspection::BoundingBox& b) const;
};

// ============================================================================
// Reference Encoder (Selection → Reference)
// ============================================================================

/**
 * ReferenceEncoder - Convert inspected geometry to stable references
 */
class ReferenceEncoder {
public:
    ReferenceEncoder(inspection::Inspector* inspector);
    
    /**
     * Encode body as BodyRef
     */
    BodyRef encode_body(const inspection::BodyProperties& body);
    
    /**
     * Encode face as FaceRef
     */
    FaceRef encode_face(const inspection::FaceProperties& face);
    
    /**
     * Encode edge as EdgeRef
     */
    EdgeRef encode_edge(const inspection::EdgeProperties& edge);
    
private:
    inspection::Inspector* inspector_;
};

// ============================================================================
// Utilities
// ============================================================================

std::string resolution_status_to_string(ResolutionStatus status);

} // namespace reference
} // namespace praxis
