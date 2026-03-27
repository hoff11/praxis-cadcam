// Test PlanHistory (EPIC 1.3 - Plan Patch Enforcement)
#include "../../include/praxis/PlanHistory.hpp"
#include "../../src/recipe/RecipeTypes.hpp"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <stdexcept>

using namespace praxis::plan;
using namespace praxis::recipe;

// Helper: Create simple resolved operation
static ResolvedOperation make_box_op(const std::string& node_id, double dx, double dy, double dz) {
    ResolvedOperation op;
    op.op = NodeOp::MakeBox;
    op.node_id = node_id;
    op.resolved_args["dx"] = dx;
    op.resolved_args["dy"] = dy;
    op.resolved_args["dz"] = dz;
    return op;
}

// Helper: Create simple resolved plan
static ResolvedPlan make_simple_plan(const std::vector<ResolvedOperation>& ops) {
    ResolvedPlan plan;
    plan.operations = ops;
    plan.engine_version = "test";
    plan.kernel_version = "test";
    plan.output_node = ops.empty() ? "" : ops.back().node_id;
    return plan;
}

void test_create_initial_version() {
    std::cout << "[Test] create_initial_version\n";
    
    PlanHistory history;
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto plan = make_simple_plan({op1});
    
    int v = history.create_initial_version("test_plan", plan, "Initial test");
    assert(v == 1);
    assert(history.plan_exists("test_plan"));
    assert(history.get_latest_version("test_plan") == 1);
    
    auto version_opt = history.get_version("test_plan", 1);
    assert(version_opt.has_value());
    assert(version_opt->version == 1);
    assert(version_opt->parent_version == -1);
    assert(version_opt->plan_id == "test_plan");
    assert(version_opt->resolved_plan.operations.size() == 1);
    
    std::cout << "  ✓ Initial version created with v=1, parent=-1\n";
}

void test_duplicate_plan_rejected() {
    std::cout << "[Test] duplicate_plan_rejected\n";
    
    PlanHistory history;
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto plan = make_simple_plan({op1});
    
    history.create_initial_version("dup_test", plan);
    
    bool threw = false;
    try {
        history.create_initial_version("dup_test", plan);
    } catch (const std::runtime_error& e) {
        threw = true;
        std::string msg = e.what();
        assert(msg.find("already exists") != std::string::npos);
    }
    assert(threw);
    
    std::cout << "  ✓ Duplicate plan creation throws error\n";
}

void test_add_step_patch() {
    std::cout << "[Test] add_step_patch\n";
    
    PlanHistory history;
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto plan = make_simple_plan({op1});
    
    history.create_initial_version("add_test", plan);
    
    // Create patch to add second cube
    PlanPatch patch;
    patch.plan_id = "add_test";
    patch.target_version = 1;
    patch.reason = "Add second cube";
    
    PatchOperation add_op;
    add_op.op_type = PatchOpType::AddStep;
    add_op.new_step = make_box_op("cube_b", 20, 20, 20);
    add_op.reason = "Adding cube_b";
    
    patch.operations.push_back(add_op);
    
    // Apply patch
    auto result = history.apply_patch(patch);
    assert(result.success);
    assert(result.new_version == 2);
    assert(result.new_plan.parent_version == 1);
    assert(result.new_plan.resolved_plan.operations.size() == 2);
    
    // Verify version 1 unchanged
    auto v1 = history.get_version("add_test", 1).value();
    assert(v1.resolved_plan.operations.size() == 1);
    
    // Verify version 2 has both cubes
    auto v2 = history.get_version("add_test", 2).value();
    assert(v2.resolved_plan.operations.size() == 2);
    assert(v2.resolved_plan.operations[0].node_id == "cube_a");
    assert(v2.resolved_plan.operations[1].node_id == "cube_b");
    
    std::cout << "  ✓ AddStep creates v2 with appended operation\n";
    std::cout << "  ✓ Version 1 remains unchanged (immutability)\n";
}

void test_modify_step_patch() {
    std::cout << "[Test] modify_step_patch\n";
    
    PlanHistory history;
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto op2 = make_box_op("cube_b", 20, 20, 20);
    auto plan = make_simple_plan({op1, op2});
    
    history.create_initial_version("modify_test", plan);
    
    // Create patch to modify cube_a dimensions
    PlanPatch patch;
    patch.plan_id = "modify_test";
    patch.target_version = 1;
    patch.reason = "Enlarge cube_a";
    
    PatchOperation modify_op;
    modify_op.op_type = PatchOpType::ModifyStep;
    modify_op.step_index = 0;
    modify_op.new_step = make_box_op("cube_a", 30, 30, 30);  // Changed dimensions
    modify_op.reason = "Making cube_a bigger";
    
    patch.operations.push_back(modify_op);
    
    // Apply patch
    auto result = history.apply_patch(patch);
    assert(result.success);
    assert(result.new_version == 2);
    
    // Verify v1 unchanged
    auto v1 = history.get_version("modify_test", 1).value();
    assert(v1.resolved_plan.operations[0].resolved_args.at("dx") == 10.0);
    
    // Verify v2 has modified dimensions
    auto v2 = history.get_version("modify_test", 2).value();
    assert(v2.resolved_plan.operations[0].resolved_args.at("dx") == 30.0);
    assert(v2.resolved_plan.operations[1].resolved_args.at("dx") == 20.0);  // cube_b unchanged
    
    std::cout << "  ✓ ModifyStep creates v2 with updated operation\n";
    std::cout << "  ✓ Version 1 remains unchanged\n";
}

void test_remove_step_patch() {
    std::cout << "[Test] remove_step_patch\n";
    
    PlanHistory history;
    // Create initial plan with 3 operations
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto op2 = make_box_op("cube_b", 20, 20, 20);
    auto op3 = make_box_op("cube_c", 30, 30, 30);
    auto plan = make_simple_plan({op1, op2, op3});
    
    history.create_initial_version("remove_test", plan);
    
    // Create patch with RemoveStep to remove middle operation
    PlanPatch patch;
    patch.plan_id = "remove_test";
    patch.target_version = 1;
    patch.reason = "Remove cube_b";
    
    PatchOperation remove_op;
    remove_op.op_type = PatchOpType::RemoveStep;
    remove_op.step_index = 1;  // Remove middle operation (cube_b)
    remove_op.reason = "Removing step";
    
    patch.operations.push_back(remove_op);
    
    // Validation should pass
    auto validation = history.validate_patch(patch);
    assert(validation.valid);
    
    // Apply should succeed
    auto result = history.apply_patch(patch);
    assert(result.success);
    assert(result.new_version == 2);
    
    // Check version 2 has only 2 operations (cube_a and cube_c)
    auto v2 = history.get_version("remove_test", 2);
    assert(v2.has_value());
    assert(v2->resolved_plan.operations.size() == 2);
    assert(v2->resolved_plan.operations[0].node_id == "cube_a");
    assert(v2->resolved_plan.operations[1].node_id == "cube_c");
    
    // Version 1 should remain unchanged (3 operations)
    auto v1 = history.get_version("remove_test", 1);
    assert(v1.has_value());
    assert(v1->resolved_plan.operations.size() == 3);
    
    std::cout << "  ✓ RemoveStep creates v2 with operation removed\n";
    std::cout << "  ✓ Version 1 remains unchanged (immutability)\n";
}

void test_invalid_target_version() {
    std::cout << "[Test] invalid_target_version\n";
    
    PlanHistory history;
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto plan = make_simple_plan({op1});
    
    history.create_initial_version("version_test", plan);
    
    // Create patch targeting non-existent version
    PlanPatch patch;
    patch.plan_id = "version_test";
    patch.target_version = 99;  // Doesn't exist
    patch.reason = "Invalid target";
    
    PatchOperation add_op;
    add_op.op_type = PatchOpType::AddStep;
    add_op.new_step = make_box_op("cube_b", 10, 10, 10);
    patch.operations.push_back(add_op);
    
    // Validation should fail
    auto validation = history.validate_patch(patch);
    assert(!validation.valid);
    assert(validation.error_message.find("does not exist") != std::string::npos);
    
    std::cout << "  ✓ Invalid target version rejected\n";
}

void test_invalid_step_index() {
    std::cout << "[Test] invalid_step_index\n";
    
    PlanHistory history;
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto plan = make_simple_plan({op1});
    
    history.create_initial_version("index_test", plan);
    
    // Test ModifyStep with out-of-bounds step index
    PlanPatch patch;
    patch.plan_id = "index_test";
    patch.target_version = 1;
    patch.reason = "Invalid modify index";
    
    PatchOperation modify_op;
    modify_op.op_type = PatchOpType::ModifyStep;
    modify_op.step_index = 10;  // Out of bounds
    modify_op.new_step = make_box_op("cube_a", 30, 30, 30);
    patch.operations.push_back(modify_op);
    
    // Validation should fail
    auto validation = history.validate_patch(patch);
    assert(!validation.valid);
    assert(validation.error_message.find("out of bounds") != std::string::npos);
    
    std::cout << "  ✓ Out-of-bounds ModifyStep index rejected\n";
    
    // Test RemoveStep with out-of-bounds step index (EPIC 1.6.3)
    PlanPatch remove_patch;
    remove_patch.plan_id = "index_test";
    remove_patch.target_version = 1;
    remove_patch.reason = "Invalid remove index";
    
    PatchOperation remove_op;
    remove_op.op_type = PatchOpType::RemoveStep;
    remove_op.step_index = 10;  // Out of bounds
    remove_patch.operations.push_back(remove_op);
    
    auto remove_validation = history.validate_patch(remove_patch);
    assert(!remove_validation.valid);
    assert(remove_validation.error_message.find("out of bounds") != std::string::npos);
    
    std::cout << "  ✓ Out-of-bounds RemoveStep index rejected\n";
}

void test_list_versions() {
    std::cout << "[Test] list_versions\n";
    
    PlanHistory history;
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto plan = make_simple_plan({op1});
    
    history.create_initial_version("list_test", plan);
    
    // Add version 2
    PlanPatch patch1;
    patch1.plan_id = "list_test";
    patch1.target_version = 1;
    patch1.reason = "Add v2";
    PatchOperation add_op1;
    add_op1.op_type = PatchOpType::AddStep;
    add_op1.new_step = make_box_op("cube_b", 10, 10, 10);
    patch1.operations.push_back(add_op1);
    history.apply_patch(patch1);
    
    // Add version 3
    PlanPatch patch2;
    patch2.plan_id = "list_test";
    patch2.target_version = 2;
    patch2.reason = "Add v3";
    PatchOperation add_op2;
    add_op2.op_type = PatchOpType::AddStep;
    add_op2.new_step = make_box_op("cube_c", 10, 10, 10);
    patch2.operations.push_back(add_op2);
    history.apply_patch(patch2);
    
    // List versions
    auto versions = history.list_versions("list_test");
    assert(versions.size() == 3);
    assert(versions[0] == 1);
    assert(versions[1] == 2);
    assert(versions[2] == 3);
    
    assert(history.get_latest_version("list_test") == 3);
    
    std::cout << "  ✓ Version listing works correctly\n";
    std::cout << "  ✓ Latest version tracking correct\n";
}

void test_multiple_operations_in_patch() {
    std::cout << "[Test] multiple_operations_in_patch\n";
    
    PlanHistory history;
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto op2 = make_box_op("cube_b", 20, 20, 20);
    auto plan = make_simple_plan({op1, op2});
    
    history.create_initial_version("multi_op_test", plan);
    
    // Create patch with multiple operations
    PlanPatch patch;
    patch.plan_id = "multi_op_test";
    patch.target_version = 1;
    patch.reason = "Multiple changes";
    
    // Modify cube_a
    PatchOperation modify_op;
    modify_op.op_type = PatchOpType::ModifyStep;
    modify_op.step_index = 0;
    modify_op.new_step = make_box_op("cube_a", 15, 15, 15);
    patch.operations.push_back(modify_op);
    
    // Add cube_c
    PatchOperation add_op;
    add_op.op_type = PatchOpType::AddStep;
    add_op.new_step = make_box_op("cube_c", 5, 5, 5);
    patch.operations.push_back(add_op);
    
    // Apply patch
    auto result = history.apply_patch(patch);
    assert(result.success);
    
    // Verify v2 has all three cubes with correct dimensions
    auto v2 = history.get_version("multi_op_test", 2).value();
    assert(v2.resolved_plan.operations.size() == 3);
    assert(v2.resolved_plan.operations[0].node_id == "cube_a");
    assert(v2.resolved_plan.operations[0].resolved_args.at("dx") == 15.0);
    assert(v2.resolved_plan.operations[1].node_id == "cube_b");
    assert(v2.resolved_plan.operations[1].resolved_args.at("dx") == 20.0);
    assert(v2.resolved_plan.operations[2].node_id == "cube_c");
    
    std::cout << "  ✓ Multiple operations in single patch work correctly\n";
}

void test_list_plans() {
    std::cout << "[Test] list_plans\n";
    
    PlanHistory history;
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto plan = make_simple_plan({op1});
    
    history.create_initial_version("plan_a", plan);
    history.create_initial_version("plan_b", plan);
    history.create_initial_version("plan_c", plan);
    
    auto plans = history.list_plans();
    assert(plans.size() == 3);
    assert(std::find(plans.begin(), plans.end(), "plan_a") != plans.end());
    assert(std::find(plans.begin(), plans.end(), "plan_b") != plans.end());
    assert(std::find(plans.begin(), plans.end(), "plan_c") != plans.end());
    
    std::cout << "  ✓ Plan listing works correctly\n";
}

int main() {
    std::cout << "=== EPIC 1.3 Plan History Tests ===\n\n";
    
    test_create_initial_version();
    test_duplicate_plan_rejected();
    test_add_step_patch();
    test_modify_step_patch();
    test_remove_step_patch();  // EPIC 1.6.3: RemoveStep now allowed
    test_invalid_target_version();
    test_invalid_step_index();
    test_list_versions();
    test_multiple_operations_in_patch();
    test_list_plans();
    
    std::cout << "\n=== All tests passed ✓ ===\n";
    return 0;
}
