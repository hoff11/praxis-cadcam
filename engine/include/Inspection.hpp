/**
 * Inspection.hpp
 * 
 * Read-only inspection API for Praxis Semantic Interaction Contract (SIC)
 * Sprint 7 - Epic 3: Kernel Adapter Compliance
 * 
 * Purpose: Expose minimal inspection surface required by selectors and references
 * Contract: Per kernel_requirements.md - deterministic enumeration and property access only
 * 
 * FORBIDDEN:
 * - Mutation APIs
 * - Sketch/feature/history access
 * - Non-deterministic enumeration
 * - UI-coupled dependencies
 */

#pragma once

#include <vector>
#include <string>
#include <optional>
#include <memory>

namespace praxis {
namespace inspection {

// ============================================================================
// Geometric Primitives
// ============================================================================

struct Vector3 {
    double x, y, z;
    
    Vector3() : x(0), y(0), z(0) {}
    Vector3(double x, double y, double z) : x(x), y(y), z(z) {}
};

struct BoundingBox {
    double min_x, min_y, min_z;
    double max_x, max_y, max_z;
    
    BoundingBox() 
        : min_x(0), min_y(0), min_z(0)
        , max_x(0), max_y(0), max_z(0) {}
};

// ============================================================================
// Surface and Edge Classification
// ============================================================================

// Face location within topology (body-scoped)
struct FaceLocation {
    int body_index;
    int local_face_index;  // 0-based within the body
    
    bool operator==(const FaceLocation& other) const {
        return body_index == other.body_index && local_face_index == other.local_face_index;
    }
};

enum class SurfaceType {
    PLANAR,
    CYLINDRICAL,
    CONICAL,
    SPHERICAL,
    TOROIDAL,
    BSPLINE,
    OTHER
};

enum class EdgeType {
    LINE,
    CIRCULAR,
    ELLIPTICAL,
    BSPLINE,
    OTHER
};

// String conversion helpers
inline SurfaceType string_to_surface_type(const std::string& str) {
    if (str == "planar") return SurfaceType::PLANAR;
    if (str == "cylindrical") return SurfaceType::CYLINDRICAL;
    if (str == "conical") return SurfaceType::CONICAL;
    if (str == "spherical") return SurfaceType::SPHERICAL;
    if (str == "toroidal") return SurfaceType::TOROIDAL;
    if (str == "bspline") return SurfaceType::BSPLINE;
    return SurfaceType::OTHER;
}

inline EdgeType string_to_edge_type(const std::string& str) {
    if (str == "line") return EdgeType::LINE;
    if (str == "circular") return EdgeType::CIRCULAR;
    if (str == "elliptical") return EdgeType::ELLIPTICAL;
    if (str == "bspline") return EdgeType::BSPLINE;
    return EdgeType::OTHER;
}

// ============================================================================
// Inspectable Properties (Read-Only)
// ============================================================================

struct BodyProperties {
    int index;
    double volume;
    Vector3 centroid;
    BoundingBox bbox;
    std::string signature; // Stable signature derived from geometric invariants
};

struct FaceProperties {
    int index;
    int body_index;
    SurfaceType surface_type;
    double area;
    Vector3 centroid;
    Vector3 normal;  // Normal vector (primarily for planar faces)
    BoundingBox bbox;
    std::string signature; // Stable signature derived from surface/area/centroid/normal
};

struct EdgeProperties {
    int index;
    std::vector<FaceLocation> adjacent_faces;  // Body-scoped face references
    std::vector<int> vertex_indices;           // Endpoint vertices (v2.0 contract)
    EdgeType edge_type;
    double length;
    double radius;  // For circular edges (0 for non-circular)
    Vector3 midpoint;
    BoundingBox bbox;
    std::string signature; // Stable signature derived from type/length/radius/midpoint
};

struct VertexProperties {
    int index;
    std::vector<int> edge_indices;
    Vector3 point;
    std::string signature; // Stable signature derived from point coordinates
};

// ============================================================================
// Inspection Result
// ============================================================================

struct ArtifactInspection {
    std::string artifact_path;
    int body_count;
    std::vector<BodyProperties> bodies;
    std::vector<FaceProperties> faces;
    std::vector<EdgeProperties> edges;
    std::vector<VertexProperties> vertices;
    std::string timestamp;
};

// ============================================================================
// Inspector Interface (Kernel Adapter)
// ============================================================================

/**
 * Inspector - Deterministic, read-only geometry inspection
 * 
 * Contract guarantees:
 * - Enumeration order is stable for identical geometry
 * - Properties are computed consistently
 * - No mutation paths are exposed
 * - No sketch/history/feature APIs are accessible
 */
class Inspector {
public:
    virtual ~Inspector() = default;
    
    /**
     * Load artifact from STEP file
     * Returns true on success
     */
    virtual bool load_artifact(const std::string& path) = 0;
    
    /**
     * Get artifact path
     */
    virtual std::string get_artifact_path() const = 0;
    
    /**
     * Enumerate bodies deterministically
     * Order must be stable across loads of identical geometry
     */
    virtual std::vector<BodyProperties> enumerate_bodies() = 0;
    
    /**
     * Enumerate faces for a specific body deterministically
     */
    virtual std::vector<FaceProperties> enumerate_faces(int body_index) = 0;
    
    /**
     * Enumerate all faces across all bodies deterministically
     */
    virtual std::vector<FaceProperties> enumerate_all_faces() = 0;
    
    /**
     * Enumerate edges for a specific face deterministically
     */
    virtual std::vector<EdgeProperties> enumerate_edges(int face_index) = 0;
    
    /**
     * Enumerate all edges across all faces deterministically
     */
    virtual std::vector<EdgeProperties> enumerate_all_edges() = 0;
    
    /**
     * Enumerate all vertices deterministically (deduplicated)
     */
    virtual std::vector<VertexProperties> enumerate_all_vertices() = 0;
    
    /**
     * Perform full inspection
     */
    virtual ArtifactInspection inspect() = 0;
};

// ============================================================================
// Factory
// ============================================================================

/**
 * Create inspector instance
 * Implementation will be wired to OCCT kernel in Epic 3
 */
std::unique_ptr<Inspector> create_inspector();

// ============================================================================
// Utilities
// ============================================================================

std::string surface_type_to_string(SurfaceType type);
std::string edge_type_to_string(EdgeType type);

SurfaceType string_to_surface_type(const std::string& str);
EdgeType string_to_edge_type(const std::string& str);

} // namespace inspection
} // namespace praxis
