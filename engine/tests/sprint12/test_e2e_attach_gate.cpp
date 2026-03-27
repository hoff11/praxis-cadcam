/**
 * test_e2e_attach_gate.cpp
 * Sprint 12 - TRUE HARD GATE (v0.6.0 Blocker)
 * 
 * PURPOSE: Validates AttachFaceToFace behavior fully
 * - MUST execute successfully (no graceful failures)
 * - MUST validate geometry transform
 * - MUST validate face coincidence
 * 
 * This is a BLOCKER for v0.6.0 - cannot pass unless attach works.
 */

#include "TestHarness.hpp"
#include "OCCTInspector.hpp"
#include "Reference.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <cmath>

using namespace praxis::testing;
using namespace praxis::inspection;
using namespace praxis::reference;

double compute_centroid_x(const BodyInfo& body) {
    return (body.bbox[0] + body.bbox[3]) / 2.0;
}

double compute_face_plane_x_max(const BodyInfo& body) {
    return body.bbox[3];  // Max X
}

double compute_face_plane_x_min(const BodyInfo& body) {
    return body.bbox[0];  // Min X
}

void test_attach_face_to_face_gate() {
    std::cout << "[GATE] attach_face_to_face_behavior\n";
    
    // Hermetic test directory
    fs::path test_dir = TestHarness::create_test_dir("attach_gate");
    fs::path plan_file = test_dir / "attach_plan.json";
    fs::path out_dir = test_dir / "output";
    
    // Step 1: Create initial two boxes without attach
    json initial_plan = json{
        {"plan_id", "gate_test_setup"},
        {"version", 1},
        {"units", "mm"},
        {"steps", {
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"size_x", "40"}, {"size_y", "40"}, {"size_z", "40"},
                    {"tx", "0"}, {"ty", "0"}, {"tz", "0"}
                }}
            },
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"size_x", "30"}, {"size_y", "30"}, {"size_z", "30"},
                    {"tx", "50"}, {"ty", "0"}, {"tz", "0"}
                }}
            }
        }}
    };
    
    {
        std::ofstream f(plan_file);
        f << initial_plan.dump(2);
        f.close();
    }
    
    std::cout << "  → Creating initial boxes\n";
    auto setup_result = TestHarness::run_plan(plan_file, out_dir);
    if (!setup_result.success()) {
        std::cerr << "  ✗ Failed to create initial boxes\n";
        assert(false && "Setup must succeed");
    }
    
    // Step 2: Encode FaceRefs from the pre-attach state
    fs::path preattach_state = out_dir / "state.step";
    TestHarness::assert_file_exists(preattach_state, "pre-attach state");
    
    auto inspector = create_inspector();
    if (!inspector->load_artifact(preattach_state.string())) {
        std::cerr << "  ✗ Failed to load pre-attach state for encoding\n";
        assert(false && "Must load artifact");
    }
    
    ReferenceEncoder encoder(inspector.get());
    
    // Encode face refs: Body 0 eastward face (+X max centroid), Body 1 westward face (+X min centroid)
    auto body0_faces = inspector->enumerate_faces(0);
    auto body1_faces = inspector->enumerate_faces(1);
    
    // Find eastmost face of body 0 (normal pointing +X)
    FaceRef fixed_face;
    double max_x = -1e9;
    int fixed_idx = -1;
    for (size_t i = 0; i < body0_faces.size(); i++) {
        const auto& f = body0_faces[i];
        if (f.normal.x > 0.5 && f.centroid.x > max_x) {
            max_x = f.centroid.x;
            fixed_idx = i;
        }
    }
    if (fixed_idx >= 0) {
        fixed_face = encoder.encode_face(body0_faces[fixed_idx]);
        std::cout << "  ✓ Found body 0 +X face (index=" << fixed_idx 
                  << ", centroid.x=" << body0_faces[fixed_idx].centroid.x 
                  << ", normal=(" << body0_faces[fixed_idx].normal.x << "," 
                  << body0_faces[fixed_idx].normal.y << "," << body0_faces[fixed_idx].normal.z << "))\n";
    } else {
        std::cerr << "  ✗ Could not find +X face on body 0\n";
        assert(false && "Test setup failed");
    }
    
    // Find westmost face of body 1 (smallest centroid.x, X-normal face)
    // Note: OCCT normals point outward from solids, so westmost face has normal (+1,0,0) not (-1,0,0)
    FaceRef moving_face;
    double min_x = 1e9;
    int moving_idx = -1;
    for (size_t i = 0; i < body1_faces.size(); i++) {
        const auto& f = body1_faces[i];
        // X-aligned face (normal pointing ±X)
        if (std::abs(f.normal.x) > 0.5 && f.centroid.x < min_x) {
            min_x = f.centroid.x;
            moving_idx = i;
        }
    }
    if (moving_idx >= 0) {
        moving_face = encoder.encode_face(body1_faces[moving_idx]);
        std::cout << "  ✓ Found body 1 westward face (index=" << moving_idx 
                  << ", centroid.x=" << body1_faces[moving_idx].centroid.x 
                  << ", normal=(" << body1_faces[moving_idx].normal.x << "," 
                  << body1_faces[moving_idx].normal.y << "," << body1_faces[moving_idx].normal.z << "))\n";
    } else {
        std::cerr << "  ✗ Could not find westward face on body 1\n";
        assert(false && "Test setup failed");
    }
    
    std::string moving_json = moving_face.to_json();
    std::string fixed_json = fixed_face.to_json();
    
    // Capture bodies BEFORE attach for comparison
    auto bodies_before = TestHarness::inspect_bodies(preattach_state);
    if (bodies_before.size() != 2) {
        std::cerr << "  ✗ Expected 2 bodies before attach, got " << bodies_before.size() << "\n";
        assert(false);
    }
    
    // Step 3: Create attach plan with properly encoded FaceRefs
    json attach_plan = json{
        {"plan_id", "gate_test_attach"},
        {"version", 1},
        {"units", "mm"},
        {"steps", {
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"size_x", "40"}, {"size_y", "40"}, {"size_z", "40"},
                    {"tx", "0"}, {"ty", "0"}, {"tz", "0"}
                }}
            },
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"size_x", "30"}, {"size_y", "30"}, {"size_z", "30"},
                    {"tx", "50"}, {"ty", "0"}, {"tz", "0"}
                }}
            },
            {
                {"op_type", "AttachFaceToFace"},
                {"args", {
                    {"moving_face", moving_json},
                    {"fixed_face", fixed_json}
                }}
            }
        }}
    };
    
    {
        std::ofstream f(plan_file);
        f << attach_plan.dump(2);
        f.close();
    }
    
    std::cout << "  → Running praxis-cad plan (GATE - must succeed)\n";
    auto result = TestHarness::run_plan(plan_file, out_dir);
    
    // HARD GATE: MUST succeed
    if (!result.success()) {
        auto error = TestHarness::parse_error_json(result);
        std::cerr << "  ✗ GATE FAILURE: Attach plan failed (exit " << result.exit_code << ")\n";
        if (error.has_error()) {
            std::cerr << "  Error code: " << error.code << "\n";
            std::cerr << "  Message: " << error.message << "\n";
        }
        std::cerr << "STDOUT:\n" << result.stdout_output << "\n";
        std::cerr << "STDERR:\n" << result.stderr_output << "\n";
        assert(false && "AttachFaceToFace MUST succeed for v0.6.0");
    }
    
    std::cout << "  ✓ Plan executed successfully\n";
    
    // HARD GATE: Validate geometry
    fs::path final_state = out_dir / "state.step";
    TestHarness::assert_file_exists(final_state, "final state.step");
    
    fs::path step2_dir = out_dir / "step_2";
    TestHarness::assert_file_exists(step2_dir, "step_2 directory (AttachFaceToFace output)");
    
    auto bodies_after = TestHarness::inspect_bodies(final_state);
    
    // HARD GATE: Must have 2 bodies
    if (bodies_after.size() != 2) {
        std::cerr << "  ✗ Expected 2 bodies after attach, got " << bodies_after.size() << "\n";
        assert(false && "Body count must be 2 after attach");
    }
    std::cout << "  ✓ Final state contains 2 bodies\n";
    
    // Identify bodies by volume (40³=64000, 30³=27000)
    fixed_idx = -1;
    moving_idx = -1;
    for (size_t i = 0; i < bodies_after.size(); i++) {
        if (bodies_after[i].volume > 60000) fixed_idx = i;
        else if (bodies_after[i].volume > 20000) moving_idx = i;
    }
    
    if (fixed_idx == -1 || moving_idx == -1) {
        std::cerr << "  ✗ Could not identify fixed/moving bodies by volume\n";
        assert(false);
    }
    
    std::cout << "  ✓ Bodies identified: fixed (volume=" << bodies_after[fixed_idx].volume 
              << ") and moving (volume=" << bodies_after[moving_idx].volume << ")\n";
    
    // HARD GATE: Verify geometric correctness
    const BodyInfo& fixed_before = bodies_before[fixed_idx];
    const BodyInfo& moving_before = bodies_before[moving_idx];
    const BodyInfo& fixed_after = bodies_after[fixed_idx];
    const BodyInfo& moving_after = bodies_after[moving_idx];
    
    // Fixed body must not move
    double fixed_centroid_before_x = compute_centroid_x(fixed_before);
    double fixed_centroid_after_x = compute_centroid_x(fixed_after);
    double fixed_delta = std::abs(fixed_centroid_after_x - fixed_centroid_before_x);
    
    if (fixed_delta > 0.01) {
        std::cerr << "  ✗ Fixed body moved (centroid delta: " << fixed_delta << "mm)\n";
        assert(false && "Fixed body must remain stationary");
    }
    std::cout << "  ✓ Fixed body stationary (centroid delta: " << fixed_delta << "mm)\n";
    
    // Moving body must move
    double moving_centroid_before_x = compute_centroid_x(moving_before);
    double moving_centroid_after_x = compute_centroid_x(moving_after);
    double moving_delta = std::abs(moving_centroid_after_x - moving_centroid_before_x);
    
    if (moving_delta < 1.0) {
        std::cerr << "  ✗ Moving body did not move significantly (delta: " << moving_delta << "mm)\n";
        assert(false && "Moving body must translate to attach");
    }
    std::cout << "  ✓ Moving body translated (centroid delta: " << moving_delta << "mm)\n";
    
    // Face planes must be coincident (gap < 0.1mm for flush mate)
    double fixed_face_x = compute_face_plane_x_max(fixed_after);   // Eastmost face of fixed
    double moving_face_x = compute_face_plane_x_min(moving_after); // Westmost face of moving
    double face_gap = std::abs(fixed_face_x - moving_face_x);
    
    if (face_gap > 0.1) {
        std::cerr << "  ✗ Face gap too large: " << face_gap << "mm (expected < 0.1mm)\n";
        std::cerr << "  Fixed face at X=" << fixed_face_x << ", moving face at X=" << moving_face_x << "\n";
        assert(false && "Faces must be coincident for flush mate");
    }
    std::cout << "  ✓ Faces coincident (gap: " << face_gap << "mm)\n";
    
    std::cout << "\n✅ AttachFaceToFace GATE PASSED\n";
    std::cout << "   - Plan executed successfully\n";
    std::cout << "   - AttachFaceToFace operation completed\n";
    std::cout << "   - 2 bodies preserved in final state\n";
    std::cout << "   - Fixed body remained stationary\n";
    std::cout << "   - Moving body translated correctly\n";
    std::cout << "   - Face planes are coincident (flush mate)\n";
}

int main() {
    std::cout << "\n=== Sprint 12: GATE - AttachFaceToFace Behavior ===\n\n";
    
    try {
        test_attach_face_to_face_gate();
        std::cout << "\n✅ v0.6.0 GATE PASSED\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ GATE FAILED: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n❌ GATE FAILED: unknown exception\n";
        return 1;
    }
}
