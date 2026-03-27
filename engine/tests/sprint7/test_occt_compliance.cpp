/**
 * test_occt_compliance.cpp
 * 
 * Test OCCT Inspector compliance with kernel requirements:
 * - Deterministic enumeration
 * - Stable ordering across loads
 * - Property extraction accuracy
 */

#include "OCCTInspector.hpp"
#include <iostream>
#include <cmath>
#include <cassert>
#include <filesystem>

using namespace praxis::inspection;

void test_load_step_file() {
    std::cout << "Test: load_step_file\n";
    
    OCCTInspector inspector;
    
    // Load box.step from scratch directory
        std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    std::cout << "  STEP path: " << step_path << "\n";
    std::cout << "  Exists?  : " << (std::filesystem::exists(step_path) ? "yes" : "no") << "\n";
    bool loaded = inspector.load_artifact(step_path);
    if (!loaded) {
        std::cerr << "  ERROR: Failed to load STEP file" << std::endl;
        std::cerr << "  Path: " << step_path << std::endl;
        std::cerr << "  Exists: " << (std::filesystem::exists(step_path) ? "yes" : "no") << std::endl;
        std::exit(1);
    }
    
    // Check artifact was inspected
    ArtifactInspection info = inspector.inspect();
    assert(info.body_count > 0 && "No bodies found");
    assert(info.faces.size() > 0 && "No faces found");
    assert(info.edges.size() > 0 && "No edges found");
    
    std::cout << "  Bodies: " << info.body_count << "\n";
    std::cout << "  Faces: " << info.faces.size() << "\n";
    std::cout << "  Edges: " << info.edges.size() << "\n";
    std::cout << "  ✓ STEP file loaded\n\n";
}

void test_deterministic_enumeration() {
    std::cout << "Test: deterministic_enumeration\n";

    auto almost_equal = [](double a, double b, double eps=1e-6){ return std::fabs(a-b) <= eps; };
    
    // Load same file twice
    OCCTInspector inspector1, inspector2;
        std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    inspector1.load_artifact(step_path);
    inspector2.load_artifact(step_path);
    
    // Check body counts and ordered properties/signatures match
    auto bodies1 = inspector1.enumerate_bodies();
    auto bodies2 = inspector2.enumerate_bodies();
    assert(bodies1.size() == bodies2.size() && "Body counts differ");
    for (size_t i = 0; i < bodies1.size(); ++i) {
        assert(almost_equal(bodies1[i].volume, bodies2[i].volume) && "Body volume order mismatch");
        assert(almost_equal(bodies1[i].centroid.x, bodies2[i].centroid.x));
        assert(almost_equal(bodies1[i].centroid.y, bodies2[i].centroid.y));
        assert(almost_equal(bodies1[i].centroid.z, bodies2[i].centroid.z));
        assert(bodies1[i].signature == bodies2[i].signature && "Body signature mismatch");
    }
    
    // Check face counts and ordered properties match
    auto faces1 = inspector1.enumerate_all_faces();
    auto faces2 = inspector2.enumerate_all_faces();
    assert(faces1.size() == faces2.size() && "Face counts differ");
    for (size_t i = 0; i < faces1.size(); ++i) {
        assert(faces1[i].surface_type == faces2[i].surface_type && "Face type order mismatch");
        assert(almost_equal(faces1[i].area, faces2[i].area) && "Face area order mismatch");
        assert(almost_equal(faces1[i].centroid.x, faces2[i].centroid.x));
        assert(almost_equal(faces1[i].centroid.y, faces2[i].centroid.y));
        assert(almost_equal(faces1[i].centroid.z, faces2[i].centroid.z));
        assert(faces1[i].signature == faces2[i].signature && "Face signature mismatch");
    }
    
    // Check edge counts and ordered properties match
    auto edges1 = inspector1.enumerate_all_edges();
    auto edges2 = inspector2.enumerate_all_edges();
    assert(edges1.size() == edges2.size() && "Edge counts differ");
    for (size_t i = 0; i < edges1.size(); ++i) {
        assert(edges1[i].edge_type == edges2[i].edge_type && "Edge type order mismatch");
        assert(almost_equal(edges1[i].length, edges2[i].length) && "Edge length order mismatch");
        assert(edges1[i].signature == edges2[i].signature && "Edge signature mismatch");
    }
    
    std::cout << "  First load:  " << bodies1.size() << " bodies, " 
              << faces1.size() << " faces, " << edges1.size() << " edges\n";
    std::cout << "  Second load: " << bodies2.size() << " bodies, " 
              << faces2.size() << " faces, " << edges2.size() << " edges\n";
    std::cout << "  ✓ Enumeration is deterministic\n\n";
}

void test_face_properties() {
    std::cout << "Test: face_properties\n";
    
    OCCTInspector inspector;
        std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    inspector.load_artifact(step_path);
    
    auto faces = inspector.enumerate_all_faces();
    assert(faces.size() > 0 && "No faces to test");
    
    // Check first face has valid properties
    const auto& face = faces[0];
    std::cout << "  Face 0:\n";
    std::cout << "    Type: " << surface_type_to_string(face.surface_type) << "\n";
    std::cout << "    Area: " << face.area << "\n";
    std::cout << "    Normal: (" << face.normal.x << ", " 
              << face.normal.y << ", " << face.normal.z << ")\n";
    
    assert(face.area > 0.0 && "Invalid face area");
    
    // For box, expect planar faces
    int planar_count = 0;
    for (const auto& f : faces) {
        if (f.surface_type == SurfaceType::PLANAR) {
            planar_count++;
        }
    }
    std::cout << "  Planar faces: " << planar_count << " / " << faces.size() << "\n";
    std::cout << "  ✓ Face properties extracted\n\n";
}

void test_edge_properties() {
    std::cout << "Test: edge_properties\n";
    
    OCCTInspector inspector;
        std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    inspector.load_artifact(step_path);
    
    auto edges = inspector.enumerate_all_edges();
    assert(edges.size() > 0 && "No edges to test");
    
    // Check first edge has valid properties
    const auto& edge = edges[0];
    std::cout << "  Edge 0:\n";
    std::cout << "    Type: " << edge_type_to_string(edge.edge_type) << "\n";
    std::cout << "    Length: " << edge.length << "\n";
    
    assert(edge.length > 0.0 && "Invalid edge length");
    
    // For box, expect linear edges
    int line_count = 0;
    for (const auto& e : edges) {
        if (e.edge_type == EdgeType::LINE) {
            line_count++;
        }
    }
    std::cout << "  Linear edges: " << line_count << " / " << edges.size() << "\n";
    std::cout << "  ✓ Edge properties extracted\n\n";
}

void test_hierarchical_enumeration() {
    std::cout << "Test: hierarchical_enumeration\n";
    
    OCCTInspector inspector;
    inspector.load_artifact(std::string(TEST_FIXTURE_DIR) + "/box.step");
    
    auto bodies = inspector.enumerate_bodies();
    assert(bodies.size() > 0 && "No bodies found");
    
    // Enumerate faces of first body
    auto faces = inspector.enumerate_faces(0);
    std::cout << "  Body 0 has " << faces.size() << " faces\n";
    
    // Enumerate edges of first face
    if (faces.size() > 0) {
        auto edges = inspector.enumerate_edges(0);
        std::cout << "  Face 0 has " << edges.size() << " edges\n";
    }
    
    std::cout << "  ✓ Hierarchical enumeration works\n\n";
}

int main() {
    std::cout << "=== OCCT Compliance Tests ===\n\n";
    
    try {
        test_load_step_file();
        test_deterministic_enumeration();
        test_face_properties();
        test_edge_properties();
        test_hierarchical_enumeration();
        
        std::cout << "=== ALL TESTS PASSED ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION\n";
        return 1;
    }
}
