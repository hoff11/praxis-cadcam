/**
 * FakeGeometry.hpp
 * 
 * Test-only fake geometry model for Epic 1 filter execution validation.
 * 
 * Purpose: Prove selector semantics are correct before OCCT integration.
 * 
 * IMPORTANT: This is NOT production code. It will be replaced by real
 * OCCT enumeration in Epic 3. Keep it minimal and test-scoped only.
 */

#pragma once

#include "Inspection.hpp"
#include <vector>
#include <string>

namespace praxis {
namespace testing {

// ============================================================================
// Fake Geometry (Test-Only)
// ============================================================================

struct FakeFace {
    int index;
    int body_index;
    std::string surface_type;  // "planar", "cylindrical", etc.
    double area;
    inspection::Vector3 centroid;
    inspection::Vector3 normal;  // For planar faces
    
    // Convert to inspection format
    inspection::FaceProperties to_inspection() const {
        inspection::FaceProperties props;
        props.index = index;
        props.body_index = body_index;
        props.surface_type = inspection::string_to_surface_type(surface_type);
        props.area = area;
        props.centroid = centroid;
        props.normal = normal;
        return props;
    }
};

struct FakeEdge {
    int index;
    std::string edge_type;  // "line", "circular", etc.
    double length;
    double radius;  // For circular edges
    inspection::Vector3 midpoint;
    
    // Convert to inspection format
    inspection::EdgeProperties to_inspection() const {
        inspection::EdgeProperties props;
        props.index = index;
        props.edge_type = inspection::string_to_edge_type(edge_type);
        props.length = length;
        props.radius = radius;
        props.midpoint = midpoint;
        return props;
    }
};

struct FakeBody {
    int index;
    double volume;
    inspection::Vector3 centroid;
    inspection::BoundingBox bbox;
    
    // Convert to inspection format
    inspection::BodyProperties to_inspection() const {
        inspection::BodyProperties props;
        props.index = index;
        props.volume = volume;
        props.centroid = centroid;
        props.bbox = bbox;
        return props;
    }
};

// ============================================================================
// Test Fixtures
// ============================================================================

/**
 * Create a simple test scene with deterministic geometry:
 * - 1 body (box)
 * - 6 faces (planar, various normals and areas)
 * - Several edges (lines and circular)
 */
struct SimpleBoxFixture {
    std::vector<FakeBody> bodies;
    std::vector<FakeFace> faces;
    std::vector<FakeEdge> edges;
    
    SimpleBoxFixture() {
        // Body: 100x100x100 box
        FakeBody body;
        body.index = 0;
        body.volume = 1000000.0;
        body.centroid = {50, 50, 50};
        body.bbox.min_x = 0;
        body.bbox.min_y = 0;
        body.bbox.min_z = 0;
        body.bbox.max_x = 100;
        body.bbox.max_y = 100;
        body.bbox.max_z = 100;
        bodies.push_back(body);
        
        // Face 0: Bottom (planar, -Z, area=10000)
        FakeFace face0;
        face0.index = 0;
        face0.body_index = 0;
        face0.surface_type = "planar";
        face0.area = 10000.0;
        face0.centroid = {50, 50, 0};
        face0.normal = {0, 0, -1};
        faces.push_back(face0);
        
        // Face 1: Top (planar, +Z, area=10000)
        FakeFace face1;
        face1.index = 1;
        face1.body_index = 0;
        face1.surface_type = "planar";
        face1.area = 10000.0;
        face1.centroid = {50, 50, 100};
        face1.normal = {0, 0, 1};
        faces.push_back(face1);
        
        // Face 2: Left (planar, -X, area=10000)
        FakeFace face2;
        face2.index = 2;
        face2.body_index = 0;
        face2.surface_type = "planar";
        face2.area = 10000.0;
        face2.centroid = {0, 50, 50};
        face2.normal = {-1, 0, 0};
        faces.push_back(face2);
        
        // Face 3: Right (planar, +X, area=10000)
        FakeFace face3;
        face3.index = 3;
        face3.body_index = 0;
        face3.surface_type = "planar";
        face3.area = 10000.0;
        face3.centroid = {100, 50, 50};
        face3.normal = {1, 0, 0};
        faces.push_back(face3);
        
        // Face 4: Front (planar, -Y, area=10000)
        FakeFace face4;
        face4.index = 4;
        face4.body_index = 0;
        face4.surface_type = "planar";
        face4.area = 10000.0;
        face4.centroid = {50, 0, 50};
        face4.normal = {0, -1, 0};
        faces.push_back(face4);
        
        // Face 5: Back (planar, +Y, area=10000)
        FakeFace face5;
        face5.index = 5;
        face5.body_index = 0;
        face5.surface_type = "planar";
        face5.area = 10000.0;
        face5.centroid = {50, 100, 50};
        face5.normal = {0, 1, 0};
        faces.push_back(face5);
        
        // Edges (simplified - box has 12 edges)
        for (int i = 0; i < 12; i++) {
            FakeEdge edge;
            edge.index = i;
            edge.edge_type = "line";
            edge.length = 100.0;
            edge.radius = 0.0;
            edge.midpoint = {50, 50, 50};  // Simplified
            edges.push_back(edge);
        }
    }
};

/**
 * Create a scene with cylindrical and varied geometry for aggregate tests
 */
struct VariedGeometryFixture {
    std::vector<FakeFace> faces;
    
    VariedGeometryFixture() {
        // Face 0: Small planar (area=100)
        FakeFace face0;
        face0.index = 0;
        face0.body_index = 0;
        face0.surface_type = "planar";
        face0.area = 100.0;
        face0.centroid = {0, 0, 0};
        face0.normal = {0, 0, 1};
        faces.push_back(face0);
        
        // Face 1: Medium planar (area=500)
        FakeFace face1;
        face1.index = 1;
        face1.body_index = 0;
        face1.surface_type = "planar";
        face1.area = 500.0;
        face1.centroid = {10, 0, 0};
        face1.normal = {0, 0, 1};
        faces.push_back(face1);
        
        // Face 2: Large planar (area=1000) - should be picked by area=max
        FakeFace face2;
        face2.index = 2;
        face2.body_index = 0;
        face2.surface_type = "planar";
        face2.area = 1000.0;
        face2.centroid = {20, 0, 0};
        face2.normal = {0, 0, 1};
        faces.push_back(face2);
        
        // Face 3: Cylindrical
        FakeFace face3;
        face3.index = 3;
        face3.body_index = 0;
        face3.surface_type = "cylindrical";
        face3.area = 750.0;
        face3.centroid = {30, 0, 0};
        face3.normal = {1, 0, 0};
        faces.push_back(face3);
    }
};

} // namespace testing
} // namespace praxis
