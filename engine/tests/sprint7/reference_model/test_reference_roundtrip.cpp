/**
 * test_reference_roundtrip.cpp
 * Minimal round-trip encode/resolve tests for Epic 2
 */

#include "OCCTInspector.hpp"
#include "Reference.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>

using namespace praxis::inspection;
using namespace praxis::reference;

static void test_roundtrip_single_body_and_face() {
    std::cout << "Test: roundtrip_single_body_and_face\n";

    // First load and encode references
    OCCTInspector enc_inspector;
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    assert(std::filesystem::exists(step_path));
    bool ok = enc_inspector.load_artifact(step_path);
    assert(ok && "Failed to load STEP for encoding");

    auto bodies = enc_inspector.enumerate_bodies();
    assert(!bodies.empty());
    auto faces0 = enc_inspector.enumerate_faces(0);
    assert(!faces0.empty());

    ReferenceEncoder encoder(&enc_inspector);
    BodyRef body_ref = encoder.encode_body(bodies[0]);
    FaceRef face_ref = encoder.encode_face(faces0[0]);

    // Reload in a fresh inspector and resolve
    OCCTInspector res_inspector;
    ok = res_inspector.load_artifact(step_path);
    assert(ok && "Failed to load STEP for resolving");

    ReferenceResolver resolver(&res_inspector);
    resolver.set_tolerance(1e-6);

    auto body_res = resolver.resolve(body_ref);
    assert(body_res.status == ResolutionStatus::Success);
    auto resolved_body = std::get<BodyRef>(*body_res.resolved);
    assert(resolved_body.index == 0);

    auto face_res = resolver.resolve(face_ref);
    assert(face_res.status == ResolutionStatus::Success);
    auto resolved_face = std::get<FaceRef>(*face_res.resolved);
    assert(resolved_face.parent.index == 0);
    assert(resolved_face.index == 0);

    std::cout << "  ✓ Single body+face roundtrip resolved\n\n";
}

static void test_roundtrip_multibody_body_and_face() {
    std::cout << "Test: roundtrip_multibody_body_and_face\n";

    // First load and encode references
    OCCTInspector enc_inspector;
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/multi_body.step";
    assert(std::filesystem::exists(step_path));
    bool ok = enc_inspector.load_artifact(step_path);
    assert(ok && "Failed to load STEP for encoding (multi_body)");

    auto bodies = enc_inspector.enumerate_bodies();
    assert(bodies.size() >= 2);
    auto faces_b0 = enc_inspector.enumerate_faces(0);
    assert(!faces_b0.empty());

    ReferenceEncoder encoder(&enc_inspector);
    BodyRef body0_ref = encoder.encode_body(bodies[0]);
    FaceRef face0_ref = encoder.encode_face(faces_b0[0]);

    // Reload and resolve
    OCCTInspector res_inspector;
    ok = res_inspector.load_artifact(step_path);
    assert(ok && "Failed to load STEP for resolving (multi_body)");

    ReferenceResolver resolver(&res_inspector);
    resolver.set_tolerance(1e-6);

    auto body_res = resolver.resolve(body0_ref);
    assert(body_res.status == ResolutionStatus::Success);
    auto resolved_body = std::get<BodyRef>(*body_res.resolved);
    assert(resolved_body.index == 0);

    auto face_res = resolver.resolve(face0_ref);
    assert(face_res.status == ResolutionStatus::Success);
    auto resolved_face = std::get<FaceRef>(*face_res.resolved);
    assert(resolved_face.parent.index == 0);
    assert(resolved_face.index >= 0);

    std::cout << "  ✓ Multibody body+face roundtrip resolved\n\n";
}

static void test_roundtrip_edge_single() {
    std::cout << "Test: roundtrip_edge_single\n";

    OCCTInspector enc_inspector;
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    assert(std::filesystem::exists(step_path));
    bool ok = enc_inspector.load_artifact(step_path);
    assert(ok && "Failed to load STEP for encoding (edge)");

    auto faces0 = enc_inspector.enumerate_faces(0);
    assert(!faces0.empty());
    auto edges_f0 = enc_inspector.enumerate_edges(faces0[0].index);
    assert(!edges_f0.empty());

    ReferenceEncoder encoder(&enc_inspector);
    EdgeRef edge_ref = encoder.encode_edge(edges_f0[0]);

    OCCTInspector res_inspector;
    ok = res_inspector.load_artifact(step_path);
    assert(ok && "Failed to load STEP for resolving (edge)");

    ReferenceResolver resolver(&res_inspector);
    resolver.set_tolerance(1e-6);

    auto edge_res = resolver.resolve(edge_ref);
    assert(edge_res.status == ResolutionStatus::Success);
    auto resolved_edge = std::get<EdgeRef>(*edge_res.resolved);
    (void)resolved_edge; // no specific index expectation beyond success

    std::cout << "  ✓ Single edge roundtrip resolved\n\n";
}

static void test_json_serialization_smoke() {
    std::cout << "Test: json_serialization_smoke\n";

    OCCTInspector inspector;
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    assert(std::filesystem::exists(step_path));
    bool ok = inspector.load_artifact(step_path);
    assert(ok && "Failed to load STEP for serialization test");

    auto bodies = inspector.enumerate_bodies();
    auto faces0 = inspector.enumerate_faces(0);
    auto edges_f0 = inspector.enumerate_edges(faces0[0].index);

    ReferenceEncoder encoder(&inspector);
    auto b = encoder.encode_body(bodies[0]);
    auto f = encoder.encode_face(faces0[0]);
    auto e = encoder.encode_edge(edges_f0[0]);

    auto bj = b.to_json();
    auto fj = f.to_json();
    auto ej = e.to_json();

    assert(bj.find("\"BodyRef\"") != std::string::npos);
    assert(fj.find("\"FaceRef\"") != std::string::npos);
    assert(ej.find("\"EdgeRef\"") != std::string::npos);

    std::cout << "  ✓ JSON serialization strings emitted\n\n";
}

int main() {
    std::cout << "=== Reference Roundtrip Tests ===\n\n";
    try {
        test_roundtrip_single_body_and_face();
        test_roundtrip_multibody_body_and_face();
        test_roundtrip_edge_single();
        test_json_serialization_smoke();
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
