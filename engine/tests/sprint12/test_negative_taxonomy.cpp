/**
 * test_negative_taxonomy.cpp
 * Sprint 12 - Negative Test Suite (Error Taxonomy)
 * 
 * HARD REQUIREMENTS:
 * - Invalid inputs produce typed errors, not crashes
 * - Errors are deterministic and include context
 * - Tests cover: Missing refs, First-op-attach, Invalid JSON
 */

#include "TestHarness.hpp"
#include <iostream>
#include <cassert>
#include <fstream>

using namespace praxis::testing;

void test_attach_as_first_op_fails() {
    std::cout << "[Negative] attach_as_first_op\n";
    
    fs::path test_dir = TestHarness::create_test_dir("negative_first_op");
    fs::path plan_file = test_dir / "plan.json";
    fs::path out_dir = test_dir / "output";
    
    // INVALID: AttachFaceToFace as first operation (no state to attach to)
    json plan = json{
        {"plan_id", "invalid_first_op"},
        {"version", 1},
        {"units", "mm"},
        {"steps", {
            {
                {"op_type", "AttachFaceToFace"},
                {"args", {
                    {"moving_face", "{\"ref_type\":\"FaceRef\",\"parent\":{\"ref_type\":\"BodyRef\",\"index\":0},\"predicate\":{\"kind\":\"PlanarFace\",\"normal\":{\"x\":1.0,\"y\":0.0,\"z\":0.0}}}"},
                    {"fixed_face", "{\"ref_type\":\"FaceRef\",\"parent\":{\"ref_type\":\"BodyRef\",\"index\":0},\"predicate\":{\"kind\":\"PlanarFace\",\"normal\":{\"x\":-1.0,\"y\":0.0,\"z\":0.0}}}"}
                }}
            }
        }}
    };
    
    {
        std::ofstream f(plan_file);
        f << plan.dump(2);
        f.close();
    }
    
    auto result = TestHarness::run_plan(plan_file, out_dir);
    
    // HARD GATE: Must fail
    if (result.success()) {
        std::cerr << "  ✗ AttachFaceToFace as first op should FAIL, but succeeded!\n";
        assert(false && "Invalid operation must be rejected");
    }
    
    // HARD GATE: Must have explicit error code
    auto error = TestHarness::parse_error_json(result);
    assert(error.has_error() && "Expected PRAXIS_ERROR_JSON, but none was emitted");
    assert(error.code == "InvalidPlanOrder" && "Expected error code InvalidPlanOrder");
    std::cout << "  ✓ Correct error code: " << error.code << "\n";
    
    std::cout << "  ✓ Operation correctly rejected (exit " << result.exit_code << ")\n";
    std::cout << "  ✅ FIRST-OP VALIDATION ENFORCED\n";
}

void test_invalid_face_ref_body_index() {
    std::cout << "[Negative] invalid_body_index\n";
    
    fs::path test_dir = TestHarness::create_test_dir("negative_invalid_index");
    fs::path plan_file = test_dir / "plan.json";
    fs::path out_dir = test_dir / "output";
    
    // Create one box, then reference body index 999
    json plan = json{
        {"plan_id", "invalid_index_test"},
        {"version", 1},
        {"units", "mm"},
        {"steps", {
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"size_x", "10"}, {"size_y", "10"}, {"size_z", "10"},
                    {"tx", "0"}, {"ty", "0"}, {"tz", "0"}
                }}
            },
            {
                {"op_type", "AttachFaceToFace"},
                {"args", {
                    {"moving_face", "{\"ref_type\":\"FaceRef\",\"parent\":{\"ref_type\":\"BodyRef\",\"index\":999},\"predicate\":{\"kind\":\"PlanarFace\",\"normal\":{\"x\":1.0,\"y\":0.0,\"z\":0.0}}}"},
                    {"fixed_face", "{\"ref_type\":\"FaceRef\",\"parent\":{\"ref_type\":\"BodyRef\",\"index\":0},\"predicate\":{\"kind\":\"PlanarFace\",\"normal\":{\"x\":-1.0,\"y\":0.0,\"z\":0.0}}}"}
                }}
            }
        }}
    };
    
    {
        std::ofstream f(plan_file);
        f << plan.dump(2);
        f.close();
    }
    
    auto result = TestHarness::run_plan(plan_file, out_dir);
    
    // HARD GATE: Must fail
    if (result.success()) {
        std::cerr << "  ✗ Invalid body index should FAIL, but succeeded!\n";
        assert(false && "Out-of-bounds index must be rejected");
    }
    
    // HARD GATE: Check for explicit error code
    auto error = TestHarness::parse_error_json(result);
    assert(error.has_error() && "Expected PRAXIS_ERROR_JSON, but none was emitted");
    assert((error.code == "InvalidReference" || error.code == "ResolutionFailed" || error.code == "IntentFailed") && "Expected reference validation error code");
    std::cout << "  ✓ Correct error code: " << error.code << "\n";
    
    std::cout << "  ✓ Invalid index rejected (exit " << result.exit_code << ")\n";
    std::cout << "  ✅ REFERENCE VALIDATION ENFORCED\n";
}

void test_malformed_json() {
    std::cout << "[Negative] malformed_json\n";
    
    fs::path test_dir = TestHarness::create_test_dir("negative_malformed_json");
    fs::path plan_file = test_dir / "plan.json";
    fs::path out_dir = test_dir / "output";
    
    // Write syntactically invalid JSON
    std::ofstream f(plan_file);
    f << "{ \"steps\": [ { \"op_type\": \"CreateBox\", INVALID } ] }";
    f.close();
    
    auto result = TestHarness::run_plan(plan_file, out_dir);
    
    // HARD GATE: Must fail
    if (result.success()) {
        std::cerr << "  ✗ Malformed JSON should FAIL, but succeeded!\n";
        assert(false && "Parse errors must be caught");
    }
    
    // HARD GATE: Check for explicit error code
    auto error = TestHarness::parse_error_json(result);
    assert(error.has_error() && "Expected PRAXIS_ERROR_JSON for malformed JSON");
    assert(error.code == "JsonParseError" && "Expected JsonParseError code");
    std::cout << "  ✓ Correct error code: " << error.code << "\n";
    
    std::cout << "  ✓ Malformed JSON rejected (exit " << result.exit_code << ")\n";
    std::cout << "  ✅ INPUT VALIDATION ENFORCED\n";
}

void test_missing_required_param() {
    std::cout << "[Negative] missing_required_param\n";
    
    fs::path test_dir = TestHarness::create_test_dir("negative_missing_param");
    fs::path plan_file = test_dir / "plan.json";
    fs::path out_dir = test_dir / "output";
    
    // CreateBox missing size params
    json plan = json{
        {"plan_id", "missing_param_test"},
        {"version", 1},
        {"units", "mm"},
        {"steps", {
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"tx", "0"}, {"ty", "0"}, {"tz", "0"}
                    // Missing size_x, size_y, size_z
                }}
            }
        }}
    };
    
    {
        std::ofstream f(plan_file);
        f << plan.dump(2);
        f.close();
    }
    
    auto result = TestHarness::run_plan(plan_file, out_dir);
    
    // HARD GATE: Must fail
    if (result.success()) {
        std::cerr << "  ✗ Missing required param should FAIL, but succeeded!\n";
        assert(false && "Schema validation must be enforced");
    }
    
    // HARD GATE: Check for explicit error code
    auto error = TestHarness::parse_error_json(result);
    assert(error.has_error() && "Expected PRAXIS_ERROR_JSON for missing parameter");
    assert((error.code == "InvalidParameter" || error.code == "MissingParameter" || error.code == "IntentFailed") && "Expected parameter error code");
    std::cout << "  ✓ Correct error code: " << error.code << "\n";
    
    std::cout << "  ✓ Missing param rejected (exit " << result.exit_code << ")\n";
    std::cout << "  ✅ SCHEMA VALIDATION ENFORCED\n";
}

void test_invalid_numeric_param() {
    std::cout << "[Negative] invalid_numeric_param\n";
    
    fs::path test_dir = TestHarness::create_test_dir("negative_invalid_numeric");
    fs::path plan_file = test_dir / "plan.json";
    fs::path out_dir = test_dir / "output";
    
    // Invalid numeric string for size_x
    json plan = json{
        {"plan_id", "invalid_numeric_test"},
        {"version", 1},
        {"units", "mm"},
        {"steps", {
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"size_x", "abc"},  // Invalid: not a number
                    {"size_y", "10"}, {"size_z", "10"},
                    {"tx", "0"}, {"ty", "0"}, {"tz", "0"}
                }}
            }
        }}
    };
    
    {
        std::ofstream f(plan_file);
        f << plan.dump(2);
        f.close();
    }
    
    auto result = TestHarness::run_plan(plan_file, out_dir);
    
    // HARD GATE: Must fail
    if (result.success()) {
        std::cerr << "  ✗ Invalid numeric param should FAIL, but succeeded!\n";
        assert(false && "Invalid numeric parameters must be rejected");
    }

    // HARD GATE: Must emit structured error JSON + typed code
    auto error = TestHarness::parse_error_json(result);
    assert(error.has_error() && "Expected PRAXIS_ERROR_JSON for invalid numeric parameter");
    assert(error.code == "InvalidParameter" && "Expected InvalidParameter code for invalid numeric");
    
    std::cout << "  ✓ Invalid numeric rejected (exit " << result.exit_code << ")\n";
    std::cout << "  ✅ PARAMETER TYPE VALIDATION ENFORCED\n";
}

void test_negative_size() {
    std::cout << "[Negative] negative_size\n";
    
    fs::path test_dir = TestHarness::create_test_dir("negative_negative_size");
    fs::path plan_file = test_dir / "plan.json";
    fs::path out_dir = test_dir / "output";
    
    // Negative size (geometrically invalid)
    json plan = json{
        {"plan_id", "negative_size_test"},
        {"version", 1},
        {"units", "mm"},
        {"steps", {
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"size_x", "-10"},  // Invalid: negative
                    {"size_y", "10"}, {"size_z", "10"},
                    {"tx", "0"}, {"ty", "0"}, {"tz", "0"}
                }}
            }
        }}
    };
    
    {
        std::ofstream f(plan_file);
        f << plan.dump(2);
        f.close();
    }
    
    auto result = TestHarness::run_plan(plan_file, out_dir);
    
    // HARD GATE: Must fail
    if (result.success()) {
        std::cerr << "  ✗ Negative size should FAIL, but succeeded!\n";
        assert(false && "Negative sizes must be rejected");
    }

    // HARD GATE: Must emit structured error JSON + typed code
    auto error = TestHarness::parse_error_json(result);
    assert(error.has_error() && "Expected PRAXIS_ERROR_JSON for negative size");
    assert(error.code == "InvalidParameter" && "Expected InvalidParameter code for negative size");
    
    std::cout << "  ✓ Negative size rejected (exit " << result.exit_code << ")\n";
    std::cout << "  ✅ GEOMETRIC VALIDATION ENFORCED\n";
}

int main() {
    std::cout << "\n=== Sprint 12: Negative Taxonomy Tests (HARD GATE) ===\n\n";
    
    try {
        test_attach_as_first_op_fails();
        std::cout << "\n";
        test_invalid_face_ref_body_index();
        std::cout << "\n";
        test_malformed_json();
        std::cout << "\n";
        test_missing_required_param();
        std::cout << "\n";
        test_invalid_numeric_param();
        std::cout << "\n";
        test_negative_size();
        
        std::cout << "\n✅ All negative taxonomy tests passed\n";
        std::cout << "   (All invalid inputs correctly rejected)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception\n";
        return 1;
    }
}
