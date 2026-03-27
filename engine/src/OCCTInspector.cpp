/**
 * OCCTInspector.cpp
 * 
 * OCCT-based deterministic geometric inspection implementation
 * Sprint 9: Migrated to canonical float formatting for deterministic output
 */

#include "OCCTInspector.hpp"
#include "praxis/CanonicalFormat.hpp"
#include <STEPControl_Reader.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <GProp_GProps.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAbs_CurveType.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Pln.hxx>
#include <gp_Circ.hxx>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <iomanip>

namespace praxis {
namespace inspection {

// ============================================================================
// Utility Functions
// ============================================================================

std::string surface_type_to_string(SurfaceType type) {
    switch (type) {
        case SurfaceType::PLANAR: return "planar";
        case SurfaceType::CYLINDRICAL: return "cylindrical";
        case SurfaceType::CONICAL: return "conical";
        case SurfaceType::SPHERICAL: return "spherical";
        case SurfaceType::TOROIDAL: return "toroidal";
        case SurfaceType::BSPLINE: return "bspline";
        case SurfaceType::OTHER: return "other";
        default: return "unknown";
    }
}

std::string edge_type_to_string(EdgeType type) {
    switch (type) {
        case EdgeType::LINE: return "line";
        case EdgeType::CIRCULAR: return "circular";
        case EdgeType::ELLIPTICAL: return "elliptical";
        case EdgeType::BSPLINE: return "bspline";
        case EdgeType::OTHER: return "other";
        default: return "unknown";
    }
}

// ============================================================================
// OCCTInspector Implementation
// ============================================================================

OCCTInspector::OCCTInspector() {}

bool OCCTInspector::load_artifact(const std::string& path) {
    artifact_path_ = path;
    
    // Load STEP file
    STEPControl_Reader reader;
    IFSelect_ReturnStatus status = reader.ReadFile(path.c_str());
    if (status != IFSelect_RetDone) {
        return false;
    }
    // Transfer to TopoDS_Shape
    reader.TransferRoots();
    shape_ = reader.OneShape();
    if (shape_.IsNull()) {
        return false;
    }
    
    // Build topology caches for deterministic enumeration
    rebuild_topology_cache();
    
    return true;
}

std::string OCCTInspector::get_artifact_path() const {
    return artifact_path_;
}

ArtifactInspection OCCTInspector::inspect() {
    ArtifactInspection result;
    result.artifact_path = artifact_path_;
    result.body_count = bodies_.size();
    
    // Populate bodies
    for (const auto& props : enumerate_bodies()) {
        result.bodies.push_back(props);
    }
    
    // Populate faces
    for (const auto& props : enumerate_all_faces()) {
        result.faces.push_back(props);
    }
    
    // Populate edges
    for (const auto& props : enumerate_all_edges()) {
        result.edges.push_back(props);
    }
    
    // Populate vertices
    for (const auto& props : enumerate_all_vertices()) {
        result.vertices.push_back(props);
    }
    
    return result;
}

void OCCTInspector::rebuild_topology_cache() {
    bodies_.clear();
    faces_.clear();
    edges_.clear();
    vertices_.clear();
    edge_index_by_tshape_.clear();
    vertex_index_by_tshape_.clear();
    face_body_index_.clear();
    face_local_index_.clear();
    face_global_index_by_tshape_.clear();

    // Bodies: Use TopExp iteration order (no sorting)
    for (TopExp_Explorer exp(shape_, TopAbs_SOLID); exp.More(); exp.Next()) {
        bodies_.push_back(exp.Current());
    }

    // Faces: Use TopExp iteration order per body, track (bodyIndex, localIndex) for each face
    for (int bodyIdx = 0; bodyIdx < static_cast<int>(bodies_.size()); ++bodyIdx) {
        const auto& body = bodies_[bodyIdx];
        int localFaceIdx = 0;
        for (TopExp_Explorer exp(body, TopAbs_FACE); exp.More(); exp.Next()) {
            const TopoDS_Shape& face = exp.Current();
            int globalIdx = static_cast<int>(faces_.size());
            
            faces_.push_back(face);
            face_body_index_.push_back(bodyIdx);
            face_local_index_.push_back(localFaceIdx);
            face_global_index_by_tshape_[face.TShape().get()] = globalIdx;
            
            localFaceIdx++;
        }
    }

    // Edges: Deduplicate by TShape, assign index in first-encounter order (no sorting)
    for (const auto& body : bodies_) {
        for (TopExp_Explorer exp(body, TopAbs_EDGE); exp.More(); exp.Next()) {
            const TopoDS_Shape& edge = exp.Current();
            const TopoDS_TShape* tshape = edge.TShape().get();
            
            // Only add if not already seen (deduplication)
            if (edge_index_by_tshape_.find(tshape) == edge_index_by_tshape_.end()) {
                int edge_index = static_cast<int>(edges_.size());
                edge_index_by_tshape_[tshape] = edge_index;
                edges_.push_back(edge);
            }
        }
    }
    
    // Vertices: Deduplicate by TShape, assign index in first-encounter order (no sorting)
    for (const auto& body : bodies_) {
        for (TopExp_Explorer exp(body, TopAbs_VERTEX); exp.More(); exp.Next()) {
            const TopoDS_Shape& vertex = exp.Current();
            const TopoDS_TShape* tshape = vertex.TShape().get();
            
            // Only add if not already seen (deduplication)
            if (vertex_index_by_tshape_.find(tshape) == vertex_index_by_tshape_.end()) {
                int vertex_index = static_cast<int>(vertices_.size());
                vertex_index_by_tshape_[tshape] = vertex_index;
                vertices_.push_back(vertex);
            }
        }
    }
    
    // Build edge→vertex connectivity map using TopExp::Vertices for each stored edge
    edge_to_vertices_.clear();
    for (size_t i = 0; i < edges_.size(); ++i) {
        const TopoDS_Edge& edge = TopoDS::Edge(edges_[i]);
        TopoDS_Vertex v1, v2;
        TopExp::Vertices(edge, v1, v2);
        
        std::set<int> unique_vertex_indices;
        
        if (!v1.IsNull()) {
            const TopoDS_TShape* v1_tshape = v1.TShape().get();
            auto it = vertex_index_by_tshape_.find(v1_tshape);
            if (it != vertex_index_by_tshape_.end()) {
                unique_vertex_indices.insert(it->second);
            }
        }
        
        if (!v2.IsNull() && !v1.IsSame(v2)) {
            const TopoDS_TShape* v2_tshape = v2.TShape().get();
            auto it = vertex_index_by_tshape_.find(v2_tshape);
            if (it != vertex_index_by_tshape_.end()) {
                unique_vertex_indices.insert(it->second);
            }
        }
        
        if (!unique_vertex_indices.empty()) {
            edge_to_vertices_[static_cast<int>(i)].assign(unique_vertex_indices.begin(), unique_vertex_indices.end());
        }
    }
}

std::vector<BodyProperties> OCCTInspector::enumerate_bodies() {
    std::vector<BodyProperties> result;
    for (size_t i = 0; i < bodies_.size(); i++) {
        result.push_back(extract_body_properties(bodies_[i], static_cast<int>(i)));
    }
    return result;
}

std::vector<FaceProperties> OCCTInspector::enumerate_faces(int body_index) {
    std::vector<FaceProperties> result;
    if (body_index < 0 || body_index >= (int)bodies_.size()) {
        return result;
    }
    const TopoDS_Shape& body = bodies_[body_index];
    // Deterministic enumeration for encoding/resolution:
    // - Preserve LOCAL face index (TopExp order within the body) as FaceProperties.index
    // - Sort output by a stable key for determinism, but NEVER rewrite fp.index to a global index.
    struct FE { std::string key; FaceProperties fp; };
    std::vector<FE> local;
    auto fmt = canonical::format_float;
    int local_idx = 0;
    for (TopExp_Explorer exp(body, TopAbs_FACE); exp.More(); exp.Next(), ++local_idx) {
        const auto& f = exp.Current();
        FaceProperties fp = extract_face_properties(f, local_idx, body_index);
        std::string key =
            "T:" + surface_type_to_string(fp.surface_type) +
            "|A:" + fmt(fp.area) +
            "|C:" + fmt(fp.centroid.x) + "," + fmt(fp.centroid.y) + "," + fmt(fp.centroid.z) +
            "|N:" + fmt(fp.normal.x) + "," + fmt(fp.normal.y) + "," + fmt(fp.normal.z);
        local.push_back({key, fp});
    }
    std::sort(local.begin(), local.end(), [](const FE& a, const FE& b){ return a.key < b.key; });
    for (const auto& e : local) result.push_back(e.fp);
    return result;
}

std::vector<FaceProperties> OCCTInspector::enumerate_all_faces() {
    std::vector<FaceProperties> result;
    for (size_t i = 0; i < faces_.size(); i++) {
        int body_index = face_body_index_[i];
        int local_index = face_local_index_[i];
        // Pass local_index as the face index for stable_id purposes
        result.push_back(extract_face_properties(faces_[i], local_index, body_index));
    }
    return result;
}

std::vector<EdgeProperties> OCCTInspector::enumerate_edges(int face_index) {
    std::vector<EdgeProperties> result;
    if (face_index < 0 || face_index >= (int)faces_.size()) {
        return result;
    }
    const TopoDS_Shape& face = faces_[face_index];
    struct EE { std::string key; TopoDS_Shape shape; };
    std::vector<EE> local;
    auto fmt = canonical::format_float;
    int ei = 0;
    for (TopExp_Explorer exp(face, TopAbs_EDGE); exp.More(); exp.Next()) {
        const auto& e = exp.Current();
        EdgeProperties ep = extract_edge_properties(e, ei);
        std::string key = "T:" + edge_type_to_string(ep.edge_type) + "|L:" + fmt(ep.length) + "|R:" + fmt(ep.radius) + "|M:" + fmt(ep.midpoint.x) + "," + fmt(ep.midpoint.y) + "," + fmt(ep.midpoint.z);
        local.push_back({key, e});
        ei++;
    }
    std::sort(local.begin(), local.end(), [](const EE& a, const EE& b){ return a.key < b.key; });
    for (const auto& e : local) {
        int gidx = -1;
        for (size_t i = 0; i < edges_.size(); ++i) {
            if (e.shape.IsEqual(edges_[i])) { gidx = static_cast<int>(i); break; }
        }
        EdgeProperties ep = extract_edge_properties(e.shape, gidx >= 0 ? gidx : 0);
        result.push_back(ep);
    }
    return result;
}

std::vector<EdgeProperties> OCCTInspector::enumerate_all_edges() {
    std::vector<EdgeProperties> result;
    for (size_t i = 0; i < edges_.size(); i++) {
        result.push_back(extract_edge_properties(edges_[i], static_cast<int>(i)));
    }
    return result;
}

std::vector<VertexProperties> OCCTInspector::enumerate_all_vertices() {
    std::vector<VertexProperties> result;
    for (size_t i = 0; i < vertices_.size(); i++) {
        result.push_back(extract_vertex_properties(vertices_[i], static_cast<int>(i)));
    }
    return result;
}

BodyProperties OCCTInspector::extract_body_properties(const TopoDS_Shape& body, int index) {
    BodyProperties props;
    props.index = index;
    
    // Volume
    GProp_GProps gprops;
    BRepGProp::VolumeProperties(body, gprops);
    props.volume = gprops.Mass();
    
    // Centroid
    gp_Pnt centroid = gprops.CentreOfMass();
    props.centroid = {centroid.X(), centroid.Y(), centroid.Z()};
    
    // Bounding box
    Bnd_Box bbox;
    BRepBndLib::Add(body, bbox);
    Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    
    props.bbox.min_x = xmin;
    props.bbox.min_y = ymin;
    props.bbox.min_z = zmin;
    props.bbox.max_x = xmax;
    props.bbox.max_y = ymax;
    props.bbox.max_z = zmax;
    // Stable signature
    auto fmt = canonical::format_float;
    props.signature = std::string("V:") + fmt(props.volume) + "|C:" + fmt(props.centroid.x) + "," + fmt(props.centroid.y) + "," + fmt(props.centroid.z);

    return props;
}

FaceProperties OCCTInspector::extract_face_properties(const TopoDS_Shape& shape, int index, int body_index) {
    FaceProperties props;
    props.index = index;
    props.body_index = body_index;
    
    TopoDS_Face face = TopoDS::Face(shape);
    
    // Surface type
    props.surface_type = classify_surface(face);
    
    // Area
    GProp_GProps gprops;
    BRepGProp::SurfaceProperties(face, gprops);
    props.area = gprops.Mass();
    
    // Centroid
    gp_Pnt centroid = gprops.CentreOfMass();
    props.centroid = {centroid.X(), centroid.Y(), centroid.Z()};
    
    // Normal (for planar faces)
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() == GeomAbs_Plane) {
        gp_Pln plane = surface.Plane();
        gp_Dir normal = plane.Axis().Direction();
        props.normal = {normal.X(), normal.Y(), normal.Z()};
    } else {
        props.normal = {0, 0, 0};
    }

    // Bounding box
    {
        Bnd_Box bbox;
        BRepBndLib::Add(shape, bbox);
        Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
        bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        props.bbox.min_x = xmin;
        props.bbox.min_y = ymin;
        props.bbox.min_z = zmin;
        props.bbox.max_x = xmax;
        props.bbox.max_y = ymax;
        props.bbox.max_z = zmax;
    }
    // Stable signature
    auto fmt = canonical::format_float;
    props.signature = std::string("T:") + surface_type_to_string(props.surface_type)
        + "|A:" + fmt(props.area)
        + "|C:" + fmt(props.centroid.x) + "," + fmt(props.centroid.y) + "," + fmt(props.centroid.z)
        + "|N:" + fmt(props.normal.x) + "," + fmt(props.normal.y) + "," + fmt(props.normal.z);
    
    return props;
}

EdgeProperties OCCTInspector::extract_edge_properties(const TopoDS_Shape& shape, int index) {
    EdgeProperties props;
    props.index = index;
    
    TopoDS_Edge edge = TopoDS::Edge(shape);
    
    // Find adjacent faces by searching all faces that contain this edge
    const TopoDS_TShape* edge_tshape = edge.TShape().get();
    for (size_t face_global_idx = 0; face_global_idx < faces_.size(); ++face_global_idx) {
        // Check if this face contains this edge
        for (TopExp_Explorer exp(faces_[face_global_idx], TopAbs_EDGE); exp.More(); exp.Next()) {
            if (exp.Current().TShape().get() == edge_tshape) {
                FaceLocation floc;
                floc.body_index = face_body_index_[face_global_idx];
                floc.local_face_index = face_local_index_[face_global_idx];
                
                // Avoid duplicates (shouldn't happen with TShape comparison, but be safe)
                bool already_added = false;
                for (const auto& existing : props.adjacent_faces) {
                    if (existing == floc) {
                        already_added = true;
                        break;
                    }
                }
                if (!already_added) {
                    props.adjacent_faces.push_back(floc);
                }
                break; // This face is added, move to next face
            }
        }
    }
    
    // Find endpoint vertices (v2.0 contract) - use cached connectivity map
    auto it = edge_to_vertices_.find(index);
    if (it != edge_to_vertices_.end()) {
        props.vertex_indices = it->second;
    }
    
    // Edge type
    props.edge_type = classify_edge(edge);
    
    // Length
    GProp_GProps gprops;
    BRepGProp::LinearProperties(edge, gprops);
    props.length = gprops.Mass();
    
    // Radius (for circular edges)
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() == GeomAbs_Circle) {
        props.radius = curve.Circle().Radius();
    } else {
        props.radius = 0.0;
    }
    
    // Midpoint
    Standard_Real first = curve.FirstParameter();
    Standard_Real last = curve.LastParameter();
    Standard_Real mid = (first + last) / 2.0;
    gp_Pnt midpoint = curve.Value(mid);
    props.midpoint = {midpoint.X(), midpoint.Y(), midpoint.Z()};

    // Bounding box
    {
        Bnd_Box bbox;
        BRepBndLib::Add(shape, bbox);
        Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
        bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        props.bbox.min_x = xmin;
        props.bbox.min_y = ymin;
        props.bbox.min_z = zmin;
        props.bbox.max_x = xmax;
        props.bbox.max_y = ymax;
        props.bbox.max_z = zmax;
    }
    // Stable signature
    auto fmt = canonical::format_float;
    props.signature = std::string("T:") + edge_type_to_string(props.edge_type)
        + "|L:" + fmt(props.length)
        + "|R:" + fmt(props.radius)
        + "|M:" + fmt(props.midpoint.x) + "," + fmt(props.midpoint.y) + "," + fmt(props.midpoint.z);
    
    return props;
}

SurfaceType OCCTInspector::classify_surface(const TopoDS_Shape& shape) {
    TopoDS_Face face = TopoDS::Face(shape);
    BRepAdaptor_Surface surface(face);
    
    switch (surface.GetType()) {
        case GeomAbs_Plane: return SurfaceType::PLANAR;
        case GeomAbs_Cylinder: return SurfaceType::CYLINDRICAL;
        case GeomAbs_Cone: return SurfaceType::CONICAL;
        case GeomAbs_Sphere: return SurfaceType::SPHERICAL;
        case GeomAbs_Torus: return SurfaceType::TOROIDAL;
        case GeomAbs_BSplineSurface: return SurfaceType::BSPLINE;
        default: return SurfaceType::OTHER;
    }
}

EdgeType OCCTInspector::classify_edge(const TopoDS_Shape& shape) {
    TopoDS_Edge edge = TopoDS::Edge(shape);
    BRepAdaptor_Curve curve(edge);
    
    switch (curve.GetType()) {
        case GeomAbs_Line: return EdgeType::LINE;
        case GeomAbs_Circle: return EdgeType::CIRCULAR;
        case GeomAbs_Ellipse: return EdgeType::ELLIPTICAL;
        case GeomAbs_BSplineCurve: return EdgeType::BSPLINE;
        default: return EdgeType::OTHER;
    }
}

// Extract vertex properties with connectivity information
// NOTE: index parameter must be assigned AFTER deduplication and final ordering in rebuild_topology_cache()
VertexProperties OCCTInspector::extract_vertex_properties(const TopoDS_Shape& vertex_shape, int index) {
    VertexProperties props;
    props.index = index;
    
    // Extract point coordinates
    gp_Pnt point = BRep_Tool::Pnt(TopoDS::Vertex(vertex_shape));
    props.point = Vector3(point.X(), point.Y(), point.Z());
    
    // Find adjacent edges (for edge_indices connectivity)
    TopTools_IndexedDataMapOfShapeListOfShape vertexEdgeMap;
    TopExp::MapShapesAndAncestors(shape_, TopAbs_VERTEX, TopAbs_EDGE, vertexEdgeMap);
    
    if (vertexEdgeMap.Contains(vertex_shape)) {
        const TopTools_ListOfShape& edgeList = vertexEdgeMap.FindFromKey(vertex_shape);
        std::set<int> unique_edge_indices;  // Automatic deduplication
        for (TopTools_ListIteratorOfListOfShape it(edgeList); it.More(); it.Next()) {
            const TopoDS_Shape& edge = it.Value();
            // O(1) lookup using TShape pointer map
            const TopoDS_TShape* tshape = edge.TShape().get();
            auto map_it = edge_index_by_tshape_.find(tshape);
            if (map_it != edge_index_by_tshape_.end()) {
                unique_edge_indices.insert(map_it->second);
            }
        }
        // Convert set to sorted vector
        props.edge_indices.assign(unique_edge_indices.begin(), unique_edge_indices.end());
    }
    
    // Stable signature
    auto fmt = canonical::format_float;
    props.signature = "P:" + fmt(props.point.x) + "," + fmt(props.point.y) + "," + fmt(props.point.z);
    
    return props;
}

} // namespace inspection
} // namespace praxis
