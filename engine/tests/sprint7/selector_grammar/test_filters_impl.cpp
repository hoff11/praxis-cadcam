/**
 * test_filters_impl.cpp
 * 
 * Test filter predicate execution for Sprint 7 Selector Grammar
 * 
 * Acceptance Criteria (from sprint7.md):
 * - Exact match filters (=) must work deterministically
 * - Approximate match filters (~=) must use tolerance
 * - Aggregate reducers (max, min) must select correctly
 * - Compound filters must apply AND semantics
 */

#include "../../../include/Selector.hpp"
#include "../../../include/Inspection.hpp"
#include "../FakeGeometry.hpp"
#include <cassert>
#include <iostream>

using namespace praxis::selector;
using namespace praxis::testing;

// ============================================================================
// Fake Inspector for Testing
// ============================================================================

class FakeInspector : public praxis::inspection::Inspector {
public:
    SimpleBoxFixture fixture;
    
    bool load_artifact(const std::string& path) override { return true; }
    std::string get_artifact_path() const override { return "fake_artifact.step"; }
    praxis::inspection::ArtifactInspection inspect() override { return {}; }
    
    std::vector<praxis::inspection::BodyProperties> enumerate_bodies() override {
        std::vector<praxis::inspection::BodyProperties> bodies;
        for (const auto& fb : fixture.bodies) {
            bodies.push_back(fb.to_inspection());
        }
        return bodies;
    }
    
    std::vector<praxis::inspection::FaceProperties> enumerate_faces(int body_index) override {
        std::vector<praxis::inspection::FaceProperties> faces;
        for (const auto& ff : fixture.faces) {
            if (ff.body_index == body_index) {
                faces.push_back(ff.to_inspection());
            }
        }
        return faces;
    }
    
    std::vector<praxis::inspection::FaceProperties> enumerate_all_faces() override {
        std::vector<praxis::inspection::FaceProperties> faces;
        for (const auto& ff : fixture.faces) {
            faces.push_back(ff.to_inspection());
        }
        return faces;
    }
    
    std::vector<praxis::inspection::EdgeProperties> enumerate_edges(int face_index) override {
        std::vector<praxis::inspection::EdgeProperties> edges;
        for (const auto& fe : fixture.edges) {
            // Simplified: return all edges
            edges.push_back(fe.to_inspection());
        }
        return edges;
    }
    
    std::vector<praxis::inspection::EdgeProperties> enumerate_all_edges() override {
        std::vector<praxis::inspection::EdgeProperties> edges;
        for (const auto& fe : fixture.edges) {
            edges.push_back(fe.to_inspection());
        }
        return edges;
    }
};

// ============================================================================
// Test Cases
// ============================================================================

void test_exact_match_filter() {
    FakeInspector inspector;
    SelectorExecutor executor(&inspector);
    
    // Select all planar faces
    auto result = executor.select("feature:face?type=planar!many");
    assert(result.success);
    assert(result.references.size() == 6);  // All 6 faces are planar
    
    std::cout << "[PASS] Exact match filter (type=planar)" << std::endl;
}

void test_approximate_match_filter() {
    FakeInspector inspector;
    SelectorExecutor executor(&inspector);
    
    // Select face with normal pointing up (+Z)
    auto result = executor.select("feature:face?normal.z~=1.0!one");
    assert(result.success);
    assert(result.references.size() == 1);
    assert(result.references[0].index == 1);  // Face 1 = top face
    
    std::cout << "[PASS] Approximate match filter (normal.z~=1.0)" << std::endl;
}

void test_compound_filters() {
    FakeInspector inspector;
    SelectorExecutor executor(&inspector);
    
    // Select planar face with -Z normal (bottom)
    auto result = executor.select("feature:face?type=planar&normal.z~=-1.0!one");
    assert(result.success);
    assert(result.references.size() == 1);
    assert(result.references[0].index == 0);  // Face 0 = bottom
    
    std::cout << "[PASS] Compound filters (type=planar AND normal.z~=-1.0)" << std::endl;
}

void test_aggregate_max() {
    // Create varied geometry with different areas
    VariedGeometryFixture varied;
    
    class VariedInspector : public praxis::inspection::Inspector {
    public:
        VariedGeometryFixture* fixture;
        
        bool load_artifact(const std::string& path) override { return true; }
        std::string get_artifact_path() const override { return "fake_varied.step"; }
        praxis::inspection::ArtifactInspection inspect() override { return {}; }
        std::vector<praxis::inspection::BodyProperties> enumerate_bodies() override {
            praxis::inspection::BodyProperties body;
            body.index = 0;
            body.volume = 1000.0;
            return {body};
        }
        std::vector<praxis::inspection::FaceProperties> enumerate_faces(int body_index) override {
            std::vector<praxis::inspection::FaceProperties> faces;
            for (const auto& ff : fixture->faces) {
                faces.push_back(ff.to_inspection());
            }
            return faces;
        }
        std::vector<praxis::inspection::FaceProperties> enumerate_all_faces() override {
            std::vector<praxis::inspection::FaceProperties> faces;
            for (const auto& ff : fixture->faces) {
                faces.push_back(ff.to_inspection());
            }
            return faces;
        }
        std::vector<praxis::inspection::EdgeProperties> enumerate_edges(int face_index) override { return {}; }
        std::vector<praxis::inspection::EdgeProperties> enumerate_all_edges() override { return {}; }
    };
    
    VariedInspector inspector;
    inspector.fixture = &varied;
    SelectorExecutor executor(&inspector);
    
    // Select largest planar face
    auto result = executor.select("feature:face?type=planar&area=max!one");
    assert(result.success);
    assert(result.references.size() == 1);
    assert(result.references[0].index == 2);  // Face 2 has area=1000 (largest)
    
    std::cout << "[PASS] Aggregate max (area=max)" << std::endl;
}

void test_aggregate_min() {
    VariedGeometryFixture varied;
    
    class VariedInspector : public praxis::inspection::Inspector {
    public:
        VariedGeometryFixture* fixture;
        
        bool load_artifact(const std::string& path) override { return true; }
        std::string get_artifact_path() const override { return "fake_varied.step"; }
        praxis::inspection::ArtifactInspection inspect() override { return {}; }
        std::vector<praxis::inspection::BodyProperties> enumerate_bodies() override {
            praxis::inspection::BodyProperties body;
            body.index = 0;
            body.volume = 1000.0;
            return {body};
        }
        std::vector<praxis::inspection::FaceProperties> enumerate_faces(int body_index) override {
            std::vector<praxis::inspection::FaceProperties> faces;
            for (const auto& ff : fixture->faces) {
                faces.push_back(ff.to_inspection());
            }
            return faces;
        }
        std::vector<praxis::inspection::FaceProperties> enumerate_all_faces() override {
            std::vector<praxis::inspection::FaceProperties> faces;
            for (const auto& ff : fixture->faces) {
                faces.push_back(ff.to_inspection());
            }
            return faces;
        }
        std::vector<praxis::inspection::EdgeProperties> enumerate_edges(int face_index) override { return {}; }
        std::vector<praxis::inspection::EdgeProperties> enumerate_all_edges() override { return {}; }
    };
    
    VariedInspector inspector;
    inspector.fixture = &varied;
    SelectorExecutor executor(&inspector);
    
    // Select smallest planar face
    auto result = executor.select("feature:face?type=planar&area=min!one");
    assert(result.success);
    assert(result.references.size() == 1);
    assert(result.references[0].index == 0);  // Face 0 has area=100 (smallest)
    
    std::cout << "[PASS] Aggregate min (area=min)" << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Sprint 7: Selector Filter Tests ===" << std::endl;
    
    test_exact_match_filter();
    test_approximate_match_filter();
    test_compound_filters();
    test_aggregate_max();
    test_aggregate_min();
    
    std::cout << "\n✓ All filter tests passed!" << std::endl;
    return 0;
}
