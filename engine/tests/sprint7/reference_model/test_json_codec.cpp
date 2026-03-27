/**
 * test_json_codec.cpp
 * Comprehensive JSON codec tests for Epic 2 references
 * - Contract version validation
 * - Missing field handling
 * - Round-trip encode/decode
 */

#include "OCCTInspector.hpp"
#include "Reference.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>

using namespace praxis::inspection;
using namespace praxis::reference;

static void test_body_roundtrip() {
    std::cout << "Test: body_json_roundtrip\n";
    
    OCCTInspector inspector;
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    assert(std::filesystem::exists(step_path));
    assert(inspector.load_artifact(step_path));
    
    auto bodies = inspector.enumerate_bodies();
    assert(!bodies.empty());
    
    ReferenceEncoder encoder(&inspector);
    BodyRef original = encoder.encode_body(bodies[0]);
    
    // Encode to JSON
    std::string json = original.to_json();
    
    // Decode from JSON
    auto decoded = BodyRef::from_json(json);
    assert(decoded.has_value());
    
    // Verify fields match
    assert(decoded->contract_version == original.contract_version);
    assert(decoded->ref_type == original.ref_type);
    assert(decoded->index == original.index);
    assert(std::abs(decoded->signature.volume - original.signature.volume) < 1e-6);
    assert(std::abs(decoded->signature.centroid.x - original.signature.centroid.x) < 1e-6);
    assert(std::abs(decoded->signature.centroid.y - original.signature.centroid.y) < 1e-6);
    assert(std::abs(decoded->signature.centroid.z - original.signature.centroid.z) < 1e-6);
    
    // Re-encode and verify identical
    std::string json2 = decoded->to_json();
    assert(json == json2);
    
    std::cout << "  ✓ Body JSON round-trip passed\n\n";
}

static void test_face_roundtrip() {
    std::cout << "Test: face_json_roundtrip\n";
    
    OCCTInspector inspector;
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    assert(inspector.load_artifact(step_path));
    
    auto faces = inspector.enumerate_faces(0);
    assert(!faces.empty());
    
    ReferenceEncoder encoder(&inspector);
    FaceRef original = encoder.encode_face(faces[0]);
    
    // Encode to JSON
    std::string json = original.to_json();
    
    // Decode from JSON
    auto decoded = FaceRef::from_json(json);

    if (!decoded.has_value()) {
        std::cerr << "  ERROR: Failed to decode face JSON\n";
        std::cerr << "  Full JSON: " << json << "\n";
    }
    assert(decoded.has_value());
    
    // Verify fields match
    assert(decoded->contract_version == original.contract_version);
    assert(decoded->ref_type == original.ref_type);
    assert(decoded->index == original.index);
    assert(decoded->signature.surface_type == original.signature.surface_type);
    assert(std::abs(decoded->signature.area - original.signature.area) < 1e-6);
    assert(std::abs(decoded->signature.centroid.x - original.signature.centroid.x) < 1e-6);
    
    // Verify parent body matches
    assert(decoded->parent.index == original.parent.index);
    assert(std::abs(decoded->parent.signature.volume - original.parent.signature.volume) < 1e-6);
    
    // Re-encode and verify identical
    std::string json2 = decoded->to_json();
    assert(json == json2);
    
    std::cout << "  ✓ Face JSON round-trip passed\n\n";
}

static void test_edge_roundtrip() {
    std::cout << "Test: edge_json_roundtrip\n";
    
    OCCTInspector inspector;
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    assert(inspector.load_artifact(step_path));
    
    auto faces = inspector.enumerate_faces(0);
    auto edges = inspector.enumerate_edges(faces[0].index);
    assert(!edges.empty());
    
    ReferenceEncoder encoder(&inspector);
    EdgeRef original = encoder.encode_edge(edges[0]);
    
    // Encode to JSON
    std::string json = original.to_json();
    
    // Decode from JSON
    auto decoded = EdgeRef::from_json(json);
    assert(decoded.has_value());
    
    // Verify fields match
    assert(decoded->contract_version == original.contract_version);
    assert(decoded->ref_type == original.ref_type);
    assert(decoded->index == original.index);
    assert(decoded->signature.edge_type == original.signature.edge_type);
    assert(std::abs(decoded->signature.midpoint.x - original.signature.midpoint.x) < 1e-6);
    
    // Verify parent faces count matches
    assert(decoded->parent_faces.size() == original.parent_faces.size());
    
    // Re-encode and verify identical
    std::string json2 = decoded->to_json();
    assert(json == json2);
    
    std::cout << "  ✓ Edge JSON round-trip passed\n\n";
}

static void test_bad_contract_version() {
    std::cout << "Test: bad_contract_version\n";
    
    // Body with wrong contract version
    std::string bad_body = R"({"ref_type":"BodyRef","contract_version":"0.9","index":0,"signature":{"volume":1000,"centroid":[0,0,0],"bbox":[0,0,0,10,10,10]}})";
    auto body = BodyRef::from_json(bad_body);
    assert(!body.has_value());
    
    // Face with wrong contract version
    std::string bad_face = R"({"ref_type":"FaceRef","contract_version":"2.0","index":0,"parent":{"ref_type":"BodyRef","contract_version":"1.0","index":0,"signature":{"volume":1000,"centroid":[0,0,0],"bbox":[0,0,0,10,10,10]}},"signature":{"surface_type":"planar","area":100,"centroid":[0,0,5]}})";
    auto face = FaceRef::from_json(bad_face);
    assert(!face.has_value());
    
    std::cout << "  ✓ Bad contract version rejected\n\n";
}

static void test_missing_required_fields() {
    std::cout << "Test: missing_required_fields\n";
    
    // Body missing index
    std::string no_index = R"({"ref_type":"BodyRef","contract_version":"1.0","signature":{"volume":1000,"centroid":[0,0,0],"bbox":[0,0,0,10,10,10]}})";
    assert(!BodyRef::from_json(no_index).has_value());
    
    // Body missing volume in signature
    std::string no_volume = R"({"ref_type":"BodyRef","contract_version":"1.0","index":0,"signature":{"centroid":[0,0,0],"bbox":[0,0,0,10,10,10]}})";
    assert(!BodyRef::from_json(no_volume).has_value());
    
    // Face missing parent
    std::string no_parent = R"({"ref_type":"FaceRef","contract_version":"1.0","index":0,"signature":{"surface_type":"planar","area":100,"centroid":[0,0,5]}})";
    assert(!FaceRef::from_json(no_parent).has_value());
    
    // Face missing surface_type
    std::string no_type = R"({"ref_type":"FaceRef","contract_version":"1.0","index":0,"parent":{"ref_type":"BodyRef","contract_version":"1.0","index":0,"signature":{"volume":1000,"centroid":[0,0,0],"bbox":[0,0,0,10,10,10]}},"signature":{"area":100,"centroid":[0,0,5]}})";
    assert(!FaceRef::from_json(no_type).has_value());
    
    // Edge missing edge_type
    std::string no_edge_type = R"({"ref_type":"EdgeRef","contract_version":"1.0","index":0,"signature":{"midpoint":[0,0,0]}})";
    assert(!EdgeRef::from_json(no_edge_type).has_value());
    
    std::cout << "  ✓ Missing required fields rejected\n\n";
}

static void test_wrong_ref_type() {
    std::cout << "Test: wrong_ref_type\n";
    
    // Try to parse a BodyRef as FaceRef structure
    std::string body_as_face = R"({"ref_type":"BodyRef","contract_version":"1.0","index":0,"signature":{"volume":1000,"centroid":[0,0,0],"bbox":[0,0,0,10,10,10]}})";
    assert(!FaceRef::from_json(body_as_face).has_value());
    
    std::cout << "  ✓ Wrong ref_type rejected\n\n";
}

static void test_malformed_json() {
    std::cout << "Test: malformed_json\n";
    
    // Missing closing brace
    assert(!BodyRef::from_json(R"({"ref_type":"BodyRef","contract_version":"1.0")").has_value());
    
    // Invalid number format
    assert(!BodyRef::from_json(R"({"ref_type":"BodyRef","contract_version":"1.0","index":"not_a_number","signature":{"volume":1000,"centroid":[0,0,0],"bbox":[0,0,0,10,10,10]}})").has_value());
    
    // Malformed array
    assert(!BodyRef::from_json(R"({"ref_type":"BodyRef","contract_version":"1.0","index":0,"signature":{"volume":1000,"centroid":[0,0],"bbox":[0,0,0,10,10,10]}})").has_value());
    
    std::cout << "  ✓ Malformed JSON rejected\n\n";
}

int main() {
    std::cout << "=== JSON Codec Tests ===\n\n";
    try {
        test_body_roundtrip();
        test_face_roundtrip();
        test_edge_roundtrip();
        test_bad_contract_version();
        test_missing_required_fields();
        test_wrong_ref_type();
        test_malformed_json();
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
