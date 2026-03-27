/**
 * test_selector_resolve.cpp
 * 
 * Phase C.5: Selector Resolution Tests
 * 
 * Test Categories:
 * - Target resolution (bodies/faces/edges/vertices)
 * - Filter execution (property extraction, comparisons)
 * - Error handling (NoMatches, PropertyNotFound, TypeMismatch)
 * - Numeric tolerance and string case-insensitivity
 */

#include <catch2/catch_test_macros.hpp>
#include "SelectorResolver.hpp"
#include "Selector.hpp"
#include "Inspection.hpp"
#include <filesystem>
#include <iostream>
#include <set>

using namespace praxis::selector;
using namespace praxis::inspection;

// Test fixture paths - derive from test source location for robustness
static std::filesystem::path get_fixture_path(const std::string& filename) {
    // Get directory of this test file (source location)
    std::filesystem::path test_file = __FILE__;
    std::filesystem::path test_dir = test_file.parent_path();
    
    // Navigate to fixtures: tests/phase_c5 -> tests -> fixtures
    std::filesystem::path fixtures_dir = test_dir.parent_path() / "fixtures";
    std::filesystem::path full_path = fixtures_dir / filename;
    
    // Debug: Print path if file doesn't exist
    if (!std::filesystem::exists(full_path)) {
        std::cerr << "WARNING: Fixture not found at: " << full_path << std::endl;
        std::cerr << "  __FILE__ = " << __FILE__ << std::endl;
        std::cerr << "  test_dir = " << test_dir << std::endl;
        std::cerr << "  fixtures_dir = " << fixtures_dir << std::endl;
    }
    
    return full_path;
}

static const std::string BOX_STEP = get_fixture_path("box.step").string();
static const std::string MULTI_BODY_STEP = get_fixture_path("multi_body.step").string();

// ============================================================================
// Helper: Parse and Resolve
// ============================================================================

ResolveResult parse_and_resolve(const std::string& artifact_path, 
                                 const std::string& selector_expr) {
    // Parse selector
    auto parse_result = SelectorParser::parse(selector_expr);
    REQUIRE(parse_result.selector.has_value());
    
    // Load artifact
    auto inspector = create_inspector();
    REQUIRE(inspector->load_artifact(artifact_path));
    
    // Resolve
    SelectorResolver resolver(*inspector);
    return resolver.resolve(*parse_result.selector);
}

// ============================================================================
// Target Resolution Tests
// ============================================================================

TEST_CASE("Resolve bodies() on single box", "[phase-c5][target][bodies]") {
    auto result = parse_and_resolve(BOX_STEP, "bodies()");
    
    if (!result.success()) {
        std::cerr << "Resolution failed!" << std::endl;
        std::cerr << "Error kind: " << static_cast<int>(result.error->kind) << std::endl;
        std::cerr << "Error message: " << result.error->message << std::endl;
    }
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 1);
    REQUIRE(result.stable_ids.size() == 1);
    
    // Stable ID format: "body_0_<signature>"
    REQUIRE(result.stable_ids[0].substr(0, 5) == "body_");
}

TEST_CASE("Resolve faces() on single box", "[phase-c5][target][faces]") {
    auto result = parse_and_resolve(BOX_STEP, "faces()");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 6);  // Box has 6 faces
    REQUIRE(result.stable_ids.size() == 6);
    
    for (const auto& id : result.stable_ids) {
        // v0.5.0 format: face_b{body}_{index}_{signature}
        REQUIRE(id.substr(0, 7) == "face_b0");  // All faces belong to body 0
    }
}

TEST_CASE("Resolve edges() on single box", "[phase-c5][target][edges]") {
    auto result = parse_and_resolve(BOX_STEP, "edges()");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 12);  // Box has 12 edges
    REQUIRE(result.stable_ids.size() == 12);
    
    for (const auto& id : result.stable_ids) {
        // v0.5.0 format: edge_b{body}_{index}_{signature}
        REQUIRE(id.substr(0, 7) == "edge_b0");
    }
}

TEST_CASE("Resolve vertices() on single box", "[phase-c5][target][vertices]") {
    auto result = parse_and_resolve(BOX_STEP, "vertices()");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 8);  // Box has 8 vertices
    REQUIRE(result.stable_ids.size() == 8);
    
    for (const auto& id : result.stable_ids) {
        REQUIRE(id.substr(0, 7) == "vertex_");
    }
}

TEST_CASE("Resolve bodies() on multi-body artifact", "[phase-c5][target][multi-body]") {
    auto result = parse_and_resolve(MULTI_BODY_STEP, "bodies()");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count >= 2);  // At least 2 bodies
}

// ============================================================================
// Filter Tests - Single Property
// ============================================================================

TEST_CASE("Resolve faces(index=0)", "[phase-c5][filter][index]") {
    auto result = parse_and_resolve(BOX_STEP, "faces(index=0)");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 1);
    // v0.5.0 format includes body: face_b0_0_{sig}
    REQUIRE(result.stable_ids[0].find("face_b0_0_") == 0);
}

TEST_CASE("Resolve faces(index!=0)", "[phase-c5][filter][not-equal]") {
    auto result = parse_and_resolve(BOX_STEP, "faces(index!=0)");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 5);  // 6 faces - 1
}

TEST_CASE("Resolve edges(index<3)", "[phase-c5][filter][less-than]") {
    auto result = parse_and_resolve(BOX_STEP, "edges(index<3)");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 3);  // index 0,1,2
}

TEST_CASE("Resolve edges(index<=2)", "[phase-c5][filter][less-equal]") {
    auto result = parse_and_resolve(BOX_STEP, "edges(index<=2)");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 3);  // index 0,1,2
}

TEST_CASE("Resolve edges(index>10)", "[phase-c5][filter][greater-than]") {
    auto result = parse_and_resolve(BOX_STEP, "edges(index>10)");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 1);  // index 11 only
}

TEST_CASE("Resolve edges(index>=11)", "[phase-c5][filter][greater-equal]") {
    auto result = parse_and_resolve(BOX_STEP, "edges(index>=11)");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 1);  // index 11 only
}

// ============================================================================
// Filter Tests - Multiple Properties (AND semantics)
// ============================================================================

TEST_CASE("Resolve faces(index>0, index<3)", "[phase-c5][filter][and]") {
    auto result = parse_and_resolve(BOX_STEP, "faces(index>0, index<3)");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 2);  // index 1,2
}

TEST_CASE("Resolve edges(index>=0, index<=5)", "[phase-c5][filter][range]") {
    auto result = parse_and_resolve(BOX_STEP, "edges(index>=0, index<=5)");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 6);  // index 0,1,2,3,4,5
}

// ============================================================================
// Property Type Tests
// ============================================================================

TEST_CASE("Resolve faces(type=\"planar\")", "[phase-c5][filter][type-string]") {
    auto result = parse_and_resolve(BOX_STEP, "faces(type=\"planar\")");
    
    if (!result.success()) {
        std::cerr << "ERROR: " << result.error->message << std::endl;
    }
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 6);  // All box faces are planar
}

TEST_CASE("Resolve edges(type=\"line\")", "[phase-c5][filter][edge-type]") {
    auto result = parse_and_resolve(BOX_STEP, "edges(type=\"line\")");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 12);  // All box edges are lines
}

// ============================================================================
// Numeric Tolerance Tests
// ============================================================================

TEST_CASE("Resolve faces with area tolerance", "[phase-c5][filter][numeric-tolerance]") {
    // Test that numeric comparison uses epsilon tolerance
    auto result = parse_and_resolve(BOX_STEP, "faces(area>0.0)");
    
    REQUIRE(result.success());
    REQUIRE(result.match_count == 6);  // All faces have positive area
}

// ============================================================================
// String Case-Insensitivity Tests
// ============================================================================

TEST_CASE("Resolve faces with case-insensitive type", "[phase-c5][filter][case-insensitive]") {
    // "planar" vs "PLANAR" should match
    auto result1 = parse_and_resolve(BOX_STEP, "faces(type=\"planar\")");
    auto result2 = parse_and_resolve(BOX_STEP, "faces(type=\"PLANAR\")");
    
    REQUIRE(result1.success());
    REQUIRE(result2.success());
    REQUIRE(result1.match_count == result2.match_count);
}

// ============================================================================
// Error Tests - NoMatches
// ============================================================================

TEST_CASE("Resolve faces(index=999) returns NoMatches", "[phase-c5][error][no-matches]") {
    auto result = parse_and_resolve(BOX_STEP, "faces(index=999)");
    
    REQUIRE(!result.success());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->kind == ResolveErrorKind::NoMatches);
    REQUIRE(result.match_count == 0);
}

TEST_CASE("Resolve edges(index<0) returns NoMatches", "[phase-c5][error][no-matches]") {
    auto result = parse_and_resolve(BOX_STEP, "edges(index<0)");
    
    REQUIRE(!result.success());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->kind == ResolveErrorKind::NoMatches);
}

// ============================================================================
// Error Tests - PropertyNotFound
// ============================================================================

TEST_CASE("Resolve faces(unknown_prop=1) returns PropertyNotFound", "[phase-c5][error][property-not-found]") {
    auto result = parse_and_resolve(BOX_STEP, "faces(unknown_prop=1)");
    
    REQUIRE(!result.success());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->kind == ResolveErrorKind::PropertyNotFound);
}

TEST_CASE("Resolve bodies(area=1) returns PropertyNotFound", "[phase-c5][error][wrong-property]") {
    // Bodies don't have 'area' property (faces do)
    auto result = parse_and_resolve(BOX_STEP, "bodies(area=1)");
    
    REQUIRE(!result.success());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->kind == ResolveErrorKind::PropertyNotFound);
}

// ============================================================================
// Error Tests - TypeMismatch
// ============================================================================

TEST_CASE("Resolve faces(index=\"not_a_number\") returns TypeMismatch", "[phase-c5][error][type-mismatch]") {
    auto result = parse_and_resolve(BOX_STEP, "faces(index=\"not_a_number\")");
    
    REQUIRE(!result.success());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->kind == ResolveErrorKind::TypeMismatch);
}

// ============================================================================
// Canonical Format Tests
// ============================================================================

TEST_CASE("ResolveResult includes canonical selector", "[phase-c5][canonical]") {
    auto result = parse_and_resolve(BOX_STEP, "faces( index = 0 )");
    
    REQUIRE(result.success());
    REQUIRE(result.selector_canonical == "faces(index=0)");
}

TEST_CASE("ResolveError includes canonical selector", "[phase-c5][canonical][error]") {
    auto result = parse_and_resolve(BOX_STEP, "faces( unknown = 1 )");
    
    REQUIRE(!result.success());
    REQUIRE(result.error->selector_canonical == "faces(unknown=1)");
}

// ============================================================================
// Determinism Tests
// ============================================================================

TEST_CASE("Resolution is deterministic", "[phase-c5][determinism]") {
    auto result1 = parse_and_resolve(BOX_STEP, "faces()");
    auto result2 = parse_and_resolve(BOX_STEP, "faces()");
    
    REQUIRE(result1.success());
    REQUIRE(result2.success());
    REQUIRE(result1.stable_ids == result2.stable_ids);
}

TEST_CASE("Filter results are deterministic", "[phase-c5][determinism][filter]") {
    auto result1 = parse_and_resolve(BOX_STEP, "edges(index<5)");
    auto result2 = parse_and_resolve(BOX_STEP, "edges(index<5)");
    
    REQUIRE(result1.success());
    REQUIRE(result2.success());
    REQUIRE(result1.stable_ids == result2.stable_ids);
}
// ============================================================================
// Multi-Body Correctness Tests (v0.5.0 Critical Fix)
// ============================================================================

TEST_CASE("Multi-body faces with body filter returns correct faces", "[phase-c5][multi-body][correctness]") {
    auto result = parse_and_resolve(MULTI_BODY_STEP, "faces(body=0)");
    
    REQUIRE(result.success());
    // All returned faces should belong to body 0
    auto inspector = create_inspector();
    REQUIRE(inspector->load_artifact(MULTI_BODY_STEP));
    
    auto all_faces = inspector->enumerate_all_faces();
    for (const auto& stable_id : result.stable_ids) {
        // v0.5.0 format: face_b{body}_{index}_{sig}
        // Verify it contains b0 (body 0)
        REQUIRE(stable_id.find("face_b0_") == 0);
        
        // Find face by matching stable_id
        bool found = false;
        for (const auto& face : all_faces) {
            std::string expected_id = "face_b" + std::to_string(face.body_index) + "_" + std::to_string(face.index) + "_" + face.signature.substr(0, 16);
            if (stable_id == expected_id) {
                REQUIRE(face.body_index == 0);
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
}

TEST_CASE("Multi-body faces with body=1 filter returns different faces", "[phase-c5][multi-body][correctness]") {
    auto result = parse_and_resolve(MULTI_BODY_STEP, "faces(body=1)");
    
    REQUIRE(result.success());
    // All returned faces should belong to body 1
    auto inspector = create_inspector();
    REQUIRE(inspector->load_artifact(MULTI_BODY_STEP));
    
    auto all_faces = inspector->enumerate_all_faces();
    for (const auto& stable_id : result.stable_ids) {
        // v0.5.0 format: face_b{body}_{index}_{sig}
        REQUIRE(stable_id.find("face_b1_") == 0);
        
        bool found = false;
        for (const auto& face : all_faces) {
            std::string expected_id = "face_b" + std::to_string(face.body_index) + "_" + std::to_string(face.index) + "_" + face.signature.substr(0, 16);
            if (stable_id == expected_id) {
                REQUIRE(face.body_index == 1);
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
}

TEST_CASE("Multi-body face properties are accessed correctly (no index confusion)", "[phase-c5][multi-body][correctness]") {
    // This test would fail with the old index-parsing approach on multi-body artifacts
    // v0.5.0: stable_id now includes body_index to prevent confusion
    
    auto result = parse_and_resolve(MULTI_BODY_STEP, "faces()");
    REQUIRE(result.success());
    
    auto inspector = create_inspector();
    REQUIRE(inspector->load_artifact(MULTI_BODY_STEP));
    
    auto all_faces = inspector->enumerate_all_faces();
    
    // Verify each stable_id corresponds to the correct face properties
    for (const auto& stable_id : result.stable_ids) {
        // Extract body_index and index from v0.5.0 format: face_b{body}_{index}_{sig}
        size_t b_pos = stable_id.find("_b");
        REQUIRE(b_pos != std::string::npos);
        
        size_t next_underscore = stable_id.find('_', b_pos + 2);
        REQUIRE(next_underscore != std::string::npos);
        
        int id_body_index = std::stoi(stable_id.substr(b_pos + 2, next_underscore - (b_pos + 2)));
        
        size_t sig_underscore = stable_id.find('_', next_underscore + 1);
        REQUIRE(sig_underscore != std::string::npos);
        
        int id_index = std::stoi(stable_id.substr(next_underscore + 1, sig_underscore - (next_underscore + 1)));
        
        // Find face with this body_index and index
        bool found = false;
        for (const auto& face : all_faces) {
            if (face.index == id_index && face.body_index == id_body_index) {
                // Verify signature matches (16 chars in v0.5.0)
                std::string expected_sig = face.signature.substr(0, 16);
                std::string actual_sig = stable_id.substr(sig_underscore + 1);
                REQUIRE(actual_sig == expected_sig);
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
}

TEST_CASE("Multi-body face area filter uses correct face properties", "[phase-c5][multi-body][filter][correctness]") {
    // Filter by area - with v0.5.0 candidate-based approach this works correctly
    auto result = parse_and_resolve(MULTI_BODY_STEP, "faces(area>0)");
    
    REQUIRE(result.success());
    
    auto inspector = create_inspector();
    REQUIRE(inspector->load_artifact(MULTI_BODY_STEP));
    auto all_faces = inspector->enumerate_all_faces();
    
    // Verify each returned face actually has area > 0
    for (const auto& stable_id : result.stable_ids) {
        // Extract body_index and index from v0.5.0 format: face_b{body}_{index}_{sig}
        size_t b_pos = stable_id.find("_b");
        REQUIRE(b_pos != std::string::npos);
        
        size_t next_underscore = stable_id.find('_', b_pos + 2);
        REQUIRE(next_underscore != std::string::npos);
        
        int id_body_index = std::stoi(stable_id.substr(b_pos + 2, next_underscore - (b_pos + 2)));
        
        size_t sig_underscore = stable_id.find('_', next_underscore + 1);
        REQUIRE(sig_underscore != std::string::npos);
        
        int id_index = std::stoi(stable_id.substr(next_underscore + 1, sig_underscore - (next_underscore + 1)));
        
        bool found = false;
        for (const auto& face : all_faces) {
            if (face.index == id_index && face.body_index == id_body_index) {
                REQUIRE(face.area > 0);  // Verify the filter was applied to correct properties
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
}

TEST_CASE("Multi-body combined filters work correctly", "[phase-c5][multi-body][filter][and]") {
    // Test AND filter: body=0 and area>0
    auto result = parse_and_resolve(MULTI_BODY_STEP, "faces(body=0, area>0)");
    
    REQUIRE(result.success());
    
    auto inspector = create_inspector();
    REQUIRE(inspector->load_artifact(MULTI_BODY_STEP));
    auto all_faces = inspector->enumerate_all_faces();
    
    // Verify each face satisfies BOTH conditions
    for (const auto& stable_id : result.stable_ids) {
        // v0.5.0 format: face_b{body}_{index}_{sig}
        REQUIRE(stable_id.find("face_b0_") == 0);
        
        // Extract body_index and index
        size_t b_pos = stable_id.find("_b");
        size_t next_underscore = stable_id.find('_', b_pos + 2);
        int id_body_index = std::stoi(stable_id.substr(b_pos + 2, next_underscore - (b_pos + 2)));
        
        size_t sig_underscore = stable_id.find('_', next_underscore + 1);
        int id_index = std::stoi(stable_id.substr(next_underscore + 1, sig_underscore - (next_underscore + 1)));
        
        bool found = false;
        for (const auto& face : all_faces) {
            if (face.index == id_index && face.body_index == id_body_index) {
                REQUIRE(face.body_index == 0);
                REQUIRE(face.area > 0);
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
}

// ============================================================================
// Stable ID Uniqueness Tests (v0.5.0 Critical - Collision Prevention)
// ============================================================================

TEST_CASE("Single-body faces have unique stable IDs", "[phase-c5][stable-id][uniqueness]") {
    auto result = parse_and_resolve(BOX_STEP, "faces()");
    
    REQUIRE(result.success());
    
    // Convert to set - if there are duplicates, set size will be smaller
    std::set<std::string> unique_ids(result.stable_ids.begin(), result.stable_ids.end());
    
    REQUIRE(unique_ids.size() == result.stable_ids.size());
}

TEST_CASE("Multi-body faces have unique stable IDs (no collisions)", "[phase-c5][stable-id][uniqueness][multi-body]") {
    auto result = parse_and_resolve(MULTI_BODY_STEP, "faces()");
    
    REQUIRE(result.success());
    
    // This is the critical test: even if two bodies have identical geometry,
    // their stable IDs must be unique because they include body_index
    std::set<std::string> unique_ids(result.stable_ids.begin(), result.stable_ids.end());
    
    REQUIRE(unique_ids.size() == result.stable_ids.size());
    
    // Log sample IDs for debugging
    if (result.stable_ids.size() > 0) {
        INFO("First face stable_id: " << result.stable_ids[0]);
    }
}

TEST_CASE("Multi-body edges have unique stable IDs", "[phase-c5][stable-id][uniqueness][multi-body]") {
    auto result = parse_and_resolve(MULTI_BODY_STEP, "edges()");
    
    REQUIRE(result.success());
    
    std::set<std::string> unique_ids(result.stable_ids.begin(), result.stable_ids.end());
    REQUIRE(unique_ids.size() == result.stable_ids.size());
}

TEST_CASE("Multi-body vertices have unique stable IDs", "[phase-c5][stable-id][uniqueness][multi-body]") {
    auto result = parse_and_resolve(MULTI_BODY_STEP, "vertices()");
    
    REQUIRE(result.success());
    
    std::set<std::string> unique_ids(result.stable_ids.begin(), result.stable_ids.end());
    REQUIRE(unique_ids.size() == result.stable_ids.size());
}

TEST_CASE("Face stable IDs encode body identity", "[phase-c5][stable-id][body-scoped]") {
    auto result = parse_and_resolve(MULTI_BODY_STEP, "faces()");
    
    REQUIRE(result.success());
    
    auto inspector = create_inspector();
    REQUIRE(inspector->load_artifact(MULTI_BODY_STEP));
    auto all_faces = inspector->enumerate_all_faces();
    
    // Verify each stable_id contains the correct body_index
    for (const auto& stable_id : result.stable_ids) {
        // Extract body_index from stable_id format: face_b{body}_{index}_{sig}
        size_t b_pos = stable_id.find("_b");
        REQUIRE(b_pos != std::string::npos);
        
        size_t underscore_after_b = stable_id.find('_', b_pos + 2);
        REQUIRE(underscore_after_b != std::string::npos);
        
        std::string body_str = stable_id.substr(b_pos + 2, underscore_after_b - (b_pos + 2));
        int id_body_index = std::stoi(body_str);
        
        // Extract index
        size_t next_underscore = stable_id.find('_', underscore_after_b + 1);
        REQUIRE(next_underscore != std::string::npos);
        
        std::string index_str = stable_id.substr(underscore_after_b + 1, next_underscore - (underscore_after_b + 1));
        int id_index = std::stoi(index_str);
        
        // Find corresponding face and verify body_index matches
        bool found = false;
        for (const auto& face : all_faces) {
            if (face.index == id_index && face.body_index == id_body_index) {
                // Verify signature prefix matches (v0.5.0: 16-char prefix)
                constexpr size_t SIG_PREFIX_LEN = 16;
                std::string expected_sig = face.signature.substr(0, std::min(SIG_PREFIX_LEN, face.signature.length()));
                std::string actual_sig = stable_id.substr(next_underscore + 1);
                REQUIRE(actual_sig.substr(0, std::min(SIG_PREFIX_LEN, actual_sig.length())) == expected_sig);
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
}
