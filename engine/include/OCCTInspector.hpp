/**
 * OCCTInspector.hpp
 * 
 * OCCT-based implementation of Inspector interface for Sprint 7 Epic 3
 * 
 * Purpose: Provide deterministic, read-only geometric inspection of STEP artifacts
 * Contract: Per kernel_requirements.md - no mutation, no history, no UI coupling
 * 
 * Determinism Strategy:
 * - Use TopExp_Explorer with stable traversal order
 * - Sort entities by geometric signature when needed
 * - No memory-address-dependent ordering
 */

#pragma once

#include "Inspection.hpp"
#include <TopoDS_Shape.hxx>
#include <string>
#include <memory>
#include <unordered_map>

namespace praxis {
namespace inspection {

class OCCTInspector : public Inspector {
public:
    OCCTInspector();
    ~OCCTInspector() override = default;
    
    // Artifact loading
    bool load_artifact(const std::string& path) override;
    std::string get_artifact_path() const override;
    ArtifactInspection inspect() override;
    
    // Deterministic enumeration
    std::vector<BodyProperties> enumerate_bodies() override;
    std::vector<FaceProperties> enumerate_faces(int body_index) override;
    std::vector<FaceProperties> enumerate_all_faces() override;
    std::vector<EdgeProperties> enumerate_edges(int face_index) override;
    std::vector<EdgeProperties> enumerate_all_edges() override;
    std::vector<VertexProperties> enumerate_all_vertices() override;
    
private:
    std::string artifact_path_;
    TopoDS_Shape shape_;
    
    // Cached topology for stable indexing
    std::vector<TopoDS_Shape> bodies_;
    std::vector<TopoDS_Shape> faces_;
    std::vector<TopoDS_Shape> edges_;
    std::vector<TopoDS_Shape> vertices_;
    
    // Per-body face tracking (face indices are per-body, not global)
    std::vector<int> face_body_index_;        // face_body_index_[globalIdx] = bodyIdx
    std::vector<int> face_local_index_;       // face_local_index_[globalIdx] = localIdx within body
    std::unordered_map<const TopoDS_TShape*, int> face_global_index_by_tshape_;
    
    // Reverse lookup maps for O(1) connectivity queries
    std::unordered_map<const TopoDS_TShape*, int> edge_index_by_tshape_;
    std::unordered_map<const TopoDS_TShape*, int> vertex_index_by_tshape_;
    
    // Topology connectivity cache (built once during rebuild_topology_cache)
    std::unordered_map<int, std::vector<int>> edge_to_vertices_;  // edge_index -> vertex_indices
    
    // Rebuild topology caches
    void rebuild_topology_cache();
    
    // Property extraction
    BodyProperties extract_body_properties(const TopoDS_Shape& body, int index);
    FaceProperties extract_face_properties(const TopoDS_Shape& face, int index, int body_index);
    EdgeProperties extract_edge_properties(const TopoDS_Shape& edge, int index);
    VertexProperties extract_vertex_properties(const TopoDS_Shape& vertex, int index);
    
    // Surface type classification
    SurfaceType classify_surface(const TopoDS_Shape& face);
    EdgeType classify_edge(const TopoDS_Shape& edge);
};

} // namespace inspection
} // namespace praxis
