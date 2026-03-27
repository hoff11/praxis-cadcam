#include "../../src/recipe/RecipeTypes.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <limits>

using namespace praxis::recipe;

// Test: Verify hash is computed and non-empty
void test_hash_computed() {
    ResolvedPlan plan;
    plan.engine_version = "0.1.0";
    plan.kernel_version = "OCE-0.17";
    plan.kinematics_path_canonical = "/path/to/kinematics.yaml";
    plan.output_node = "box";
    
    // Add operation with resolved args
    ResolvedOperation op;
    op.op = NodeOp::MakeBox;
    op.node_id = "box";
    op.resolved_args["dx"] = 100.0;
    op.resolved_args["dy"] = 50.0;
    op.resolved_args["dz"] = 25.0;
    plan.operations.push_back(op);
    
    // Add final params
    plan.final_params["dx"] = 100.0;
    plan.final_params["dy"] = 50.0;
    plan.final_params["dz"] = 25.0;
    
    // Compute hash
    std::string hash = plan.compute_hash();
    
    std::cout << "Hash computed: " << hash << std::endl;
    assert(!hash.empty());
    assert(hash.length() == 64);  // SHA256 hex string = 64 chars
    
    std::cout << "✓ test_hash_computed passed\n";
}

// Test: Verify hash stability (same plan → same hash)
void test_hash_stability() {
    ResolvedPlan plan1, plan2;
    
    // Same content
    plan1.engine_version = "0.1.0";
    plan1.kernel_version = "OCE-0.17";
    plan1.kinematics_path_canonical = "/path/to/kinematics.yaml";
    plan1.output_node = "box";
    
    ResolvedOperation op;
    op.op = NodeOp::MakeBox;
    op.node_id = "box";
    op.resolved_args["dx"] = 100.0;
    op.resolved_args["dy"] = 50.0;
    op.resolved_args["dz"] = 25.0;
    plan1.operations.push_back(op);
    
    plan1.final_params["dx"] = 100.0;
    plan1.final_params["dy"] = 50.0;
    plan1.final_params["dz"] = 25.0;
    
    // Identical content
    plan2 = plan1;
    
    std::string hash1 = plan1.compute_hash();
    std::string hash2 = plan2.compute_hash();
    
    std::cout << "Hash 1: " << hash1 << std::endl;
    std::cout << "Hash 2: " << hash2 << std::endl;
    
    assert(hash1 == hash2);
    
    std::cout << "✓ test_hash_stability passed\n";
}

// Test: Verify hash sensitivity (changed param → different hash)
void test_hash_sensitivity() {
    ResolvedPlan plan1, plan2;
    
    // Setup plan1
    plan1.engine_version = "0.1.0";
    plan1.kernel_version = "OCE-0.17";
    plan1.kinematics_path_canonical = "/path/to/kinematics.yaml";
    plan1.output_node = "box";
    
    ResolvedOperation op1;
    op1.op = NodeOp::MakeBox;
    op1.node_id = "box";
    op1.resolved_args["dx"] = 100.0;
    op1.resolved_args["dy"] = 50.0;
    op1.resolved_args["dz"] = 25.0;
    plan1.operations.push_back(op1);
    
    plan1.final_params["dx"] = 100.0;
    plan1.final_params["dy"] = 50.0;
    plan1.final_params["dz"] = 25.0;
    
    // Plan2 with different dx
    plan2 = plan1;
    plan2.operations[0].resolved_args["dx"] = 101.0;  // Changed
    plan2.final_params["dx"] = 101.0;
    
    std::string hash1 = plan1.compute_hash();
    std::string hash2 = plan2.compute_hash();
    
    std::cout << "Hash 1 (dx=100): " << hash1 << std::endl;
    std::cout << "Hash 2 (dx=101): " << hash2 << std::endl;
    
    assert(hash1 != hash2);
    
    std::cout << "✓ test_hash_sensitivity passed\n";
}

// Test: Verify canonical JSON format for debugging
void test_canonical_json() {
    ResolvedPlan plan;
    plan.engine_version = "0.1.0";
    plan.kernel_version = "OCE-0.17";
    plan.kinematics_path_canonical = "/path/to/kinematics.yaml";
    plan.output_node = "box";
    
    ResolvedOperation op;
    op.op = NodeOp::MakeBox;
    op.node_id = "box";
    op.resolved_args["dx"] = 100.0;
    op.resolved_args["dy"] = 50.0;
    op.resolved_args["dz"] = 25.0;
    plan.operations.push_back(op);
    
    plan.final_params["dx"] = 100.0;
    plan.final_params["dy"] = 50.0;
    plan.final_params["dz"] = 25.0;
    
    std::string json = plan.to_canonical_json();
    
    std::cout << "Canonical JSON:\n" << json << std::endl;
    
    // Verify JSON contains key fields
    assert(json.find("\"engine_version\": \"0.1.0\"") != std::string::npos);
    assert(json.find("\"kernel_version\": \"OCE-0.17\"") != std::string::npos);
    assert(json.find("\"dx\": ") != std::string::npos);
    assert(json.find("\"dy\": ") != std::string::npos);
    assert(json.find("\"dz\": ") != std::string::npos);
    
    std::cout << "✓ test_canonical_json passed\n";
}

// Test: Validation rejects NaN (Sprint 5 Phase 3)
void test_validation_rejects_nan() {
    ResolvedPlan plan;
    plan.engine_version = "0.1.0";
    plan.kernel_version = "OCE-0.17";
    plan.kinematics_sha256 = "abc123";
    plan.output_node = "box";
    
    ResolvedOperation op;
    op.op = NodeOp::MakeBox;
    op.node_id = "box";
    op.resolved_args["dx"] = std::nan("");  // NaN!
    op.resolved_args["dy"] = 50.0;
    op.resolved_args["dz"] = 25.0;
    plan.operations.push_back(op);
    
    plan.final_params["dx"] = std::nan("");
    plan.final_params["dy"] = 50.0;
    plan.final_params["dz"] = 25.0;
    
    auto validation = ResolvedPlan::validate_for_hash(plan);
    
    assert(!validation.valid);
    assert(validation.reason.find("NaN") != std::string::npos);
    
    std::cout << "Validation correctly rejected NaN: " << validation.reason << std::endl;
    std::cout << "✓ test_validation_rejects_nan passed\n";
}

// Test: Validation rejects Infinity (Sprint 5 Phase 3)
void test_validation_rejects_infinity() {
    ResolvedPlan plan;
    plan.engine_version = "0.1.0";
    plan.kernel_version = "OCE-0.17";
    plan.kinematics_sha256 = "abc123";
    plan.output_node = "box";
    
    ResolvedOperation op;
    op.op = NodeOp::MakeBox;
    op.node_id = "box";
    op.resolved_args["dx"] = std::numeric_limits<double>::infinity();  // Infinity!
    op.resolved_args["dy"] = 50.0;
    op.resolved_args["dz"] = 25.0;
    plan.operations.push_back(op);
    
    plan.final_params["dx"] = 100.0;
    plan.final_params["dy"] = 50.0;
    plan.final_params["dz"] = 25.0;
    
    auto validation = ResolvedPlan::validate_for_hash(plan);
    
    assert(!validation.valid);
    assert(validation.reason.find("Infinity") != std::string::npos);
    
    std::cout << "Validation correctly rejected Infinity: " << validation.reason << std::endl;
    std::cout << "✓ test_validation_rejects_infinity passed\n";
}

// Test: Validation rejects missing version (Sprint 5 Phase 3)
void test_validation_rejects_missing_version() {
    ResolvedPlan plan;
    plan.engine_version = "";  // Missing!
    plan.kernel_version = "OCE-0.17";
    plan.kinematics_sha256 = "abc123";
    plan.output_node = "box";
    
    auto validation = ResolvedPlan::validate_for_hash(plan);
    
    assert(!validation.valid);
    assert(validation.reason.find("engine_version") != std::string::npos);
    
    std::cout << "Validation correctly rejected missing version: " << validation.reason << std::endl;
    std::cout << "✓ test_validation_rejects_missing_version passed\n";
}

// Test: Kinematics content affects hash (Sprint 5 Phase 3)
void test_kinematics_content_affects_hash() {
    ResolvedPlan plan1, plan2;
    
    // Same everything except kinematics_sha256
    plan1.engine_version = "0.1.0";
    plan1.kernel_version = "OCE-0.17";
    plan1.kinematics_sha256 = "aaa111";  // Different
    plan1.output_node = "box";
    
    ResolvedOperation op;
    op.op = NodeOp::MakeBox;
    op.node_id = "box";
    op.resolved_args["dx"] = 100.0;
    op.resolved_args["dy"] = 50.0;
    op.resolved_args["dz"] = 25.0;
    plan1.operations.push_back(op);
    plan1.final_params["dx"] = 100.0;
    plan1.final_params["dy"] = 50.0;
    plan1.final_params["dz"] = 25.0;
    
    // Plan2 identical except kinematics
    plan2 = plan1;
    plan2.kinematics_sha256 = "bbb222";  // Different!
    
    std::string hash1 = plan1.compute_hash();
    std::string hash2 = plan2.compute_hash();
    
    std::cout << "Hash with kinematics aaa111: " << hash1 << std::endl;
    std::cout << "Hash with kinematics bbb222: " << hash2 << std::endl;
    
    assert(hash1 != hash2);
    
    std::cout << "✓ test_kinematics_content_affects_hash passed\n";
}

int main() {
    std::cout << "\n=== Sprint 5 Phase 2+3: Plan Hash Tests ===\n\n";
    
    // Phase 2 tests
    test_hash_computed();
    test_hash_stability();
    test_hash_sensitivity();
    test_canonical_json();
    
    // Phase 3 validation tests
    test_validation_rejects_nan();
    test_validation_rejects_infinity();
    test_validation_rejects_missing_version();
    test_kinematics_content_affects_hash();
    
    std::cout << "\n✓ All hash and validation tests passed!\n";
    return 0;
}
