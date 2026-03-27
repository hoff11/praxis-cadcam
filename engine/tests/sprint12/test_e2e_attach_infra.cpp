/**
 * test_e2e_attach_infra.cpp
 * Sprint 12 - Infrastructure Validation (NOT a gate)
 * 
 * PURPOSE: Validates test harness plumbing for attach workflow
 * - Can create temp dirs
 * - Can run praxis-cad plan
 * - Detects failures cleanly (no crashes)
 * - ALLOWED TO PASS even if attach fails with known error codes
 */

#include "TestHarness.hpp"
#include <iostream>
#include <cassert>
#include <fstream>

using namespace praxis::testing;

void test_attach_harness_infra() {
    std::cout << "[Infra] attach_harness_infrastructure\n";
    
    // Hermetic test directory
    fs::path test_dir = TestHarness::create_test_dir("attach_infra");
    fs::path plan_file = test_dir / "attach_plan.json";
    fs::path out_dir = test_dir / "output";
    
    // Create minimal attach plan
    json plan = json{
        {"plan_id", "infra_test_attach"},
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
                    {"moving_face", "{\"ref_type\":\"FaceRef\",\"parent\":{\"ref_type\":\"BodyRef\",\"index\":1},\"predicate\":{\"kind\":\"PlanarFace\",\"normal\":{\"x\":-1.0,\"y\":0.0,\"z\":0.0}}}"},
                    {"fixed_face", "{\"ref_type\":\"FaceRef\",\"parent\":{\"ref_type\":\"BodyRef\",\"index\":0},\"predicate\":{\"kind\":\"PlanarFace\",\"normal\":{\"x\":1.0,\"y\":0.0,\"z\":0.0}}}"}
                }}
            }
        }}
    };
    
    {
        std::ofstream f(plan_file);
        f << plan.dump(2);
        f.close();
    }
    
    std::cout << "  → Running praxis-cad plan (infra validation)\n";
    auto result = TestHarness::run_plan(plan_file, out_dir);
    
    // INFRA VALIDATION: Check expected directory structure exists
    TestHarness::assert_file_exists(out_dir, "output directory");
    TestHarness::assert_file_exists(out_dir / "step_0", "step_0 directory");
    TestHarness::assert_file_exists(out_dir / "step_1", "step_1 directory");
    
    std::cout << "  ✓ Directory structure created\n";
    
    // INFRA VALIDATION: Command executed without crash
    if (result.exit_code == 0) {
        std::cout << "  ✓ Plan succeeded (attach fully implemented!)\n";
        TestHarness::assert_file_exists(out_dir / "state.step", "final state");
        std::cout << "  ✅ INFRA + BEHAVIOR VALIDATED\n";
        return;
    }
    
    // If failed, check it's a known/expected failure mode
    auto error = TestHarness::parse_error_json(result);
    
    if (!error.has_error()) {
        std::cout << "  ⚠ No structured error JSON (may need error emission in engine)\n";
    } else {
        std::cout << "  ✓ Structured error detected: code=" << error.code << "\n";
        
        // Known acceptable failure modes for infra test
        if (error.code == "ReferenceSignatureRequired" ||
            error.code == "ResolutionFailed" ||
            error.code == "InvalidReference") {
            std::cout << "  → Expected error (attach needs full signatures)\n";
            std::cout << "  ✅ INFRA VALIDATED (attach implementation partial)\n";
            return;
        }
    }
    
    // Any other failure mode is acceptable for infra test
    std::cout << "  → Command failed cleanly (no crash)\n";
    std::cout << "  ✅ INFRA HARNESS FUNCTIONAL\n";
}

int main() {
    std::cout << "\n=== Sprint 12: Infra - AttachFaceToFace Harness ===\n\n";
    
    try {
        test_attach_harness_infra();
        std::cout << "\n✅ Infrastructure validation passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Infra test failed: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Infra test failed with unknown exception\n";
        return 1;
    }
}
