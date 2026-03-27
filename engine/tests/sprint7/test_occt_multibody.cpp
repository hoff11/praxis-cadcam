/**
 * test_occt_multibody.cpp
 *
 * Validates deterministic enumeration on a multi-body STEP fixture.
 */

#include "OCCTInspector.hpp"
#include <iostream>
#include <cmath>
#include <cassert>
#include <filesystem>

using namespace praxis::inspection;

static bool almost_equal(double a, double b, double eps=1e-6) { return std::fabs(a-b) <= eps; }

void test_load_multi_body() {
    std::cout << "Test: load_multi_body\n";
    OCCTInspector insp;
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/multi_body.step";
    std::cout << "  STEP path: " << step_path << "\n";
    std::cout << "  Exists?  : " << (std::filesystem::exists(step_path) ? "yes" : "no") << "\n";
    bool ok = insp.load_artifact(step_path);
    assert(ok && "Failed to load multi_body.step");
    auto info = insp.inspect();
    std::cout << "  Bodies: " << info.body_count << ", Faces: " << info.faces.size() << ", Edges: " << info.edges.size() << "\n";
    assert(info.body_count >= 2 && "Expected at least 2 bodies in multi_body.step");
    std::cout << "  ✓ Loaded multi-body fixture\n\n";
}

void test_global_determinism() {
    std::cout << "Test: global_determinism\n";
    OCCTInspector a, b;
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/multi_body.step";
    assert(a.load_artifact(step_path));
    assert(b.load_artifact(step_path));

    auto bodies_a = a.enumerate_bodies();
    auto bodies_b = b.enumerate_bodies();
    assert(bodies_a.size() == bodies_b.size());
    for (size_t i = 0; i < bodies_a.size(); ++i) {
        assert(almost_equal(bodies_a[i].volume, bodies_b[i].volume));
        assert(almost_equal(bodies_a[i].centroid.x, bodies_b[i].centroid.x));
        assert(almost_equal(bodies_a[i].centroid.y, bodies_b[i].centroid.y));
        assert(almost_equal(bodies_a[i].centroid.z, bodies_b[i].centroid.z));
        assert(bodies_a[i].signature == bodies_b[i].signature);
    }

    auto faces_a = a.enumerate_all_faces();
    auto faces_b = b.enumerate_all_faces();
    assert(faces_a.size() == faces_b.size());
    for (size_t i = 0; i < faces_a.size(); ++i) {
        assert(faces_a[i].surface_type == faces_b[i].surface_type);
        assert(almost_equal(faces_a[i].area, faces_b[i].area));
        assert(almost_equal(faces_a[i].centroid.x, faces_b[i].centroid.x));
        assert(almost_equal(faces_a[i].centroid.y, faces_b[i].centroid.y));
        assert(almost_equal(faces_a[i].centroid.z, faces_b[i].centroid.z));
        assert(faces_a[i].signature == faces_b[i].signature);
    }

    auto edges_a = a.enumerate_all_edges();
    auto edges_b = b.enumerate_all_edges();
    assert(edges_a.size() == edges_b.size());
    for (size_t i = 0; i < edges_a.size(); ++i) {
        assert(edges_a[i].edge_type == edges_b[i].edge_type);
        assert(almost_equal(edges_a[i].length, edges_b[i].length));
        assert(edges_a[i].signature == edges_b[i].signature);
    }

    std::cout << "  ✓ Global enumeration deterministic across loads\n\n";
}

void test_per_body_determinism() {
    std::cout << "Test: per_body_determinism\n";
    OCCTInspector a, b;
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/multi_body.step";
    assert(a.load_artifact(step_path));
    assert(b.load_artifact(step_path));

    auto bodies_a = a.enumerate_bodies();
    auto bodies_b = b.enumerate_bodies();
    assert(bodies_a.size() == bodies_b.size());

    // For each body index, check face/edge enumeration is deterministic
    for (size_t bi = 0; bi < bodies_a.size(); ++bi) {
        auto faces_a = a.enumerate_faces(static_cast<int>(bi));
        auto faces_b = b.enumerate_faces(static_cast<int>(bi));
        assert(faces_a.size() == faces_b.size());
        for (size_t i = 0; i < faces_a.size(); ++i) {
            assert(faces_a[i].surface_type == faces_b[i].surface_type);
            assert(almost_equal(faces_a[i].area, faces_b[i].area));
            assert(faces_a[i].signature == faces_b[i].signature);
        }

        // Enumerate edges of the first face of this body to keep scope clear
        auto faces_for_body_a = a.enumerate_faces(static_cast<int>(bi));
        auto faces_for_body_b = b.enumerate_faces(static_cast<int>(bi));
        assert(!faces_for_body_a.empty() && !faces_for_body_b.empty());
        int face_idx_a = faces_for_body_a[0].index;
        int face_idx_b = faces_for_body_b[0].index;

        auto edges_a = a.enumerate_edges(face_idx_a);
        auto edges_b = b.enumerate_edges(face_idx_b);
        assert(edges_a.size() == edges_b.size());
        for (size_t i = 0; i < edges_a.size(); ++i) {
            assert(edges_a[i].edge_type == edges_b[i].edge_type);
            assert(almost_equal(edges_a[i].length, edges_b[i].length));
            assert(edges_a[i].signature == edges_b[i].signature);
        }
    }
    std::cout << "  ✓ Per-body enumeration deterministic\n\n";
}

int main() {
    std::cout << "=== OCCT Multi-Body Tests ===\n\n";
    test_load_multi_body();
    test_global_determinism();
    test_per_body_determinism();
    std::cout << "=== ALL TESTS PASSED ===\n";
    return 0;
}
