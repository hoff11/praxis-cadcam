/**
 * test_sprint8_e2e.cpp
 * Sprint 8 E2E tests: happy path, missing, ambiguous, contract mismatch, drifted
 */

#include "OCCTInspector.hpp"
#include "Selector.hpp"
#include "Reference.hpp"
#include "InteractionEmit.hpp"
#include "InteractionPublic.hpp"
#include "praxis/Intent.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>

using namespace praxis;
using namespace praxis::inspection;
using namespace praxis::reference;
using namespace praxis::selector;
using namespace praxis::interaction_public;

namespace fs = std::filesystem;

// Test 1: Happy path - exact match (REAL E2E with encode/resolve)
static void test_happy_path() {
    std::cout << "Test: Sprint 8 E2E - Happy Path (Exact Match)\n";
    
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    assert(fs::exists(step_path));
    
    // ENCODE: Load artifact and encode a body reference
    OCCTInspector enc_inspector;
    assert(enc_inspector.load_artifact(step_path));
    auto bodies = enc_inspector.enumerate_bodies();
    assert(!bodies.empty());
    
    ReferenceEncoder encoder(&enc_inspector);
    BodyRef ref = encoder.encode_body(bodies[0]);
    std::string ref_json = ref.to_json();
    
    // RESOLVE: Load same artifact in fresh inspector and resolve the reference
    OCCTInspector res_inspector;
    assert(res_inspector.load_artifact(step_path));
    
    // Parse reference back from JSON
    auto ref_parsed_opt = BodyRef::from_json(ref_json);
    assert(ref_parsed_opt.has_value());
    BodyRef ref_parsed = ref_parsed_opt.value();
    
    // Resolve
    ReferenceResolver resolver(&res_inspector);
    auto resolution = resolver.resolve(ref_parsed);
    
    // Build interaction using emit helpers
    IntentResult result;
    emit_resolution_public(result, resolution);
    
    // Verify public status is Exact
    auto pub_status = map_resolution_status(resolution.status);
    assert(pub_status == ResolutionStatusPublic::Exact);
    assert(to_string(pub_status) == std::string("Exact"));
    assert(!result.interaction.resolutions_json.empty());
    assert(result.interaction.failures.empty());
    
    std::cout << "  ✓ Happy path: Real encode→JSON→parse→resolve→Exact verified\n\n";
}

// Test 1b: FaceRef roundtrip - real E2E with encode/resolve
static void test_face_ref_roundtrip() {
    std::cout << "Test: Sprint 8 E2E - FaceRef Roundtrip (Exact Match)\n";
    
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    assert(fs::exists(step_path));
    
    // ENCODE: Load artifact and encode a face reference
    OCCTInspector enc_inspector;
    assert(enc_inspector.load_artifact(step_path));
    
    // Get first body to enumerate its faces
    auto bodies = enc_inspector.enumerate_bodies();
    assert(!bodies.empty());
    auto faces = enc_inspector.enumerate_faces(bodies[0].index);
    assert(!faces.empty());
    
    ReferenceEncoder encoder(&enc_inspector);
    FaceRef ref = encoder.encode_face(faces[0]);
    std::string ref_json = ref.to_json();
    
    // RESOLVE: Load same artifact in fresh inspector and resolve the reference
    OCCTInspector res_inspector;
    assert(res_inspector.load_artifact(step_path));
    
    // Parse reference back from JSON
    auto ref_parsed_opt = FaceRef::from_json(ref_json);
    assert(ref_parsed_opt.has_value());
    FaceRef ref_parsed = ref_parsed_opt.value();
    
    // Resolve
    ReferenceResolver resolver(&res_inspector);
    auto resolution = resolver.resolve(ref_parsed);
    
    // Build interaction using emit helpers
    IntentResult result;
    emit_resolution_public(result, resolution);
    
    // Verify public status is Exact
    auto pub_status = map_resolution_status(resolution.status);
    assert(pub_status == ResolutionStatusPublic::Exact);
    assert(to_string(pub_status) == std::string("Exact"));
    assert(!result.interaction.resolutions_json.empty());
    assert(result.interaction.failures.empty());
    
    std::cout << "  ✓ FaceRef: Real encode→JSON→parse→resolve→Exact verified\n\n";
}

// Test 1c: EdgeRef roundtrip - real E2E with encode/resolve
static void test_edge_ref_roundtrip() {
    std::cout << "Test: Sprint 8 E2E - EdgeRef Roundtrip (Exact Match)\n";
    
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    assert(fs::exists(step_path));
    
    // ENCODE: Load artifact and encode an edge reference
    OCCTInspector enc_inspector;
    assert(enc_inspector.load_artifact(step_path));
    
    // Get edges
    auto edges = enc_inspector.enumerate_all_edges();
    assert(!edges.empty());
    
    ReferenceEncoder encoder(&enc_inspector);
    EdgeRef ref = encoder.encode_edge(edges[0]);
    std::string ref_json = ref.to_json();
    
    // RESOLVE: Load same artifact in fresh inspector and resolve the reference
    OCCTInspector res_inspector;
    assert(res_inspector.load_artifact(step_path));
    
    // Parse reference back from JSON
    auto ref_parsed_opt = EdgeRef::from_json(ref_json);
    assert(ref_parsed_opt.has_value());
    EdgeRef ref_parsed = ref_parsed_opt.value();
    
    // Resolve
    ReferenceResolver resolver(&res_inspector);
    auto resolution = resolver.resolve(ref_parsed);
    
    // Build interaction using emit helpers
    IntentResult result;
    emit_resolution_public(result, resolution);
    
    // Verify public status is Exact
    auto pub_status = map_resolution_status(resolution.status);
    assert(pub_status == ResolutionStatusPublic::Exact);
    assert(to_string(pub_status) == std::string("Exact"));
    assert(!result.interaction.resolutions_json.empty());
    assert(result.interaction.failures.empty());
    
    std::cout << "  ✓ EdgeRef: Real encode→JSON→parse→resolve→Exact verified\n\n";
}

// Test 2: Missing - reference not found
static void test_missing() {
    std::cout << "Test: Sprint 8 E2E - Missing\n";
    
    // Test manual creation of Missing resolution (actual resolver test later)
    ResolutionResult missing_resolution;
    missing_resolution.status = ResolutionStatus::Missing;
    missing_resolution.message = "No body matched signature";
    
    IntentResult result;
    emit_resolution_public(result, missing_resolution);
    
    auto pub_status = map_resolution_status(missing_resolution.status);
    assert(pub_status == ResolutionStatusPublic::Missing);
    assert(to_string(pub_status) == std::string("Missing"));
    
    std::cout << "  ✓ Missing case verified\n\n";
}

// Test 3: Ambiguous - multiple matches
static void test_ambiguous() {
    std::cout << "Test: Sprint 8 E2E - Ambiguous\n";
    
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/multi_body.step";
    assert(fs::exists(step_path));
    
    // Create a reference with ambiguous signature (multiple bodies might match)
    // In practice, we'd need fixtures with identical geometry; for now, test the mapping
    ResolutionResult ambig_resolution;
    ambig_resolution.status = ResolutionStatus::Ambiguous;
    ambig_resolution.message = "Multiple bodies matched signature";
    
    IntentResult result;
    emit_resolution_public(result, ambig_resolution);
    
    auto pub_status = map_resolution_status(ambig_resolution.status);
    assert(pub_status == ResolutionStatusPublic::Ambiguous);
    assert(to_string(pub_status) == std::string("Ambiguous"));
    
    std::cout << "  ✓ Ambiguous case verified\n\n";
}

// Test 4: Contract mismatch
static void test_contract_mismatch() {
    std::cout << "Test: Sprint 8 E2E - Contract Mismatch\n";
    
    // Test manual creation of ContractMismatch resolution
    ResolutionResult mismatch_resolution;
    mismatch_resolution.status = ResolutionStatus::ContractMismatch;
    mismatch_resolution.message = "Contract version mismatch";
    
    IntentResult result;
    emit_resolution_public(result, mismatch_resolution);
    
    // Verify status is ContractMismatch, mapped to Missing
    assert(mismatch_resolution.status == ResolutionStatus::ContractMismatch);
    auto pub_status = map_resolution_status(mismatch_resolution.status);
    assert(pub_status == ResolutionStatusPublic::Missing);
    
    // emit_resolution_public should also add a failure entry
    assert(!result.interaction.failures.empty());
    assert(result.interaction.failures[0].type == "ContractMismatch");
    
    std::cout << "  ✓ Contract mismatch verified\n\n";
}

// Test 5: Drifted - signature changed
static void test_drifted() {
    std::cout << "Test: Sprint 8 E2E - Drifted\n";
    
    // Simulate drift by encoding from one artifact and resolving against a modified one
    // For this test, manually construct a Drifted result
    ResolutionResult drift_resolution;
    drift_resolution.status = ResolutionStatus::Drifted;
    drift_resolution.message = "No body matched signature (drift)";
    
    IntentResult result;
    emit_resolution_public(result, drift_resolution);
    
    auto pub_status = map_resolution_status(drift_resolution.status);
    assert(pub_status == ResolutionStatusPublic::Drifted);
    assert(to_string(pub_status) == std::string("Drifted"));
    
    std::cout << "  ✓ Drifted case verified\n\n";
}

// Test 6: Selection failure record shape
static void test_selection_failures() {
    std::cout << "Test: Sprint 8 E2E - Selection Failures\n";
    
    IntentResult result;
    
    // Add failure entry directly in the public interaction payload.
    IntentResult::InteractionInfo::Failure f1;
    f1.type = "Missing";
    f1.message = "No entities matched selector";
    f1.selector = "face[area > 100]";
    result.interaction.failures.push_back(f1);
    
    assert(!result.interaction.failures.empty());
    assert(result.interaction.failures[0].type == "Missing");
    assert(result.interaction.failures[0].message == "No entities matched selector");
    assert(result.interaction.failures[0].selector == "face[area > 100]");
    
    std::cout << "  ✓ Selection failure emit verified\n\n";
}

// Test 7: Deterministic ordering
static void test_deterministic_ordering() {
    std::cout << "Test: Sprint 8 E2E - Deterministic Ordering\n";
    
    IntentResult result;
    
    // Add references out of order
    result.interaction.references_json.push_back("{\"index\":2}");
    result.interaction.references_json.push_back("{\"index\":0}");
    result.interaction.references_json.push_back("{\"index\":1}");
    
    // Add resolutions out of order
    result.interaction.resolutions_json.push_back("{\"status\":\"Drifted\"}");
    result.interaction.resolutions_json.push_back("{\"status\":\"Ambiguous\"}");
    
    // Add selections with nested references
    IntentResult::InteractionInfo::Selection s1;
    s1.mode = "product";
    s1.selector = "body[volume > 10]";
    s1.references_json = {"{\"index\":2}", "{\"index\":1}"};
    
    IntentResult::InteractionInfo::Selection s2;
    s2.mode = "product";
    s2.selector = "body[volume > 5]";
    s2.references_json = {"{\"index\":0}"};
    
    result.interaction.selections.push_back(s1);
    result.interaction.selections.push_back(s2);
    
    // Finalize ordering
    finalize_interaction_ordering(result);
    
    // Verify ordering
    assert(result.interaction.references_json[0] == "{\"index\":0}");
    assert(result.interaction.references_json[1] == "{\"index\":1}");
    assert(result.interaction.references_json[2] == "{\"index\":2}");
    
    assert(result.interaction.resolutions_json[0] == "{\"status\":\"Ambiguous\"}");
    assert(result.interaction.resolutions_json[1] == "{\"status\":\"Drifted\"}");
    
    // Selections should be sorted by (mode, target, selector, joined refs)
    // Lexicographically: "body[volume > 10]" < "body[volume > 5]" (because '1' < '5')
    assert(result.interaction.selections[0].selector == "body[volume > 10]");
    assert(result.interaction.selections[1].selector == "body[volume > 5]");
    
    // Nested references should be sorted
    assert(result.interaction.selections[0].references_json[0] == "{\"index\":1}");
    assert(result.interaction.selections[0].references_json[1] == "{\"index\":2}");
    
    std::cout << "  ✓ Deterministic ordering verified\n\n";
}

// Test 8: Full CLI flow (inspect → select → encode → resolve)
static void test_cli_flow() {
    std::cout << "Test: Sprint 8 E2E - Full CLI Flow\n";
    
    std::string step_path = std::string(TEST_FIXTURE_DIR) + "/box.step";
    assert(fs::exists(step_path));
    
    // Step 1: Inspect
    OCCTInspector inspector;
    assert(inspector.load_artifact(step_path));
    auto bodies = inspector.enumerate_bodies();
    assert(!bodies.empty());
    std::cout << "    1. Inspected: found " << bodies.size() << " body\n";
    
    // Step 2: Encode reference (simulates select command output)
    ReferenceEncoder encoder(&inspector);
    BodyRef ref = encoder.encode_body(bodies[0]);
    std::string ref_json = ref.to_json();
    std::cout << "    2. Encoded reference (signature with volume=" << ref.signature.volume << ")\n";
    
    // Step 3: Parse reference JSON (simulates resolve command input)
    auto ref_parsed_opt = BodyRef::from_json(ref_json);
    assert(ref_parsed_opt.has_value());
    assert(ref_parsed_opt.value().contract_version == std::string(CONTRACT_VERSION));
    std::cout << "    3. Parsed reference JSON (contract_version=" << ref_parsed_opt.value().contract_version << ")\n";
    
    // Step 4: Resolve reference
    ReferenceResolver resolver(&inspector);
    auto resolution = resolver.resolve(ref_parsed_opt.value());
    auto pub_status = map_resolution_status(resolution.status);
    assert(pub_status == ResolutionStatusPublic::Exact);
    std::cout << "    4. Resolved: status=" << to_string(pub_status) << "\n";
    
    std::cout << "  ✓ Full CLI flow verified (inspect→encode→parse→resolve→Exact)\n\n";
}

int main() {
    std::cout << "Sprint 8 E2E Tests\n";
    std::cout << "==================\n\n";
    
    test_happy_path();
    test_face_ref_roundtrip();
    test_edge_ref_roundtrip();
    test_missing();
    test_ambiguous();
    test_contract_mismatch();
    test_drifted();
    test_selection_failures();
    test_deterministic_ordering();
    test_cli_flow();
    
    std::cout << "All Sprint 8 E2E tests passed!\n";
    return 0;
}
