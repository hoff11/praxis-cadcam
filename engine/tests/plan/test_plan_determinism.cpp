/**
 * test_plan_determinism.cpp
 * 
 * EPIC 1.6.3: Determinism tests for plan hash stability
 * Ensures plan_hash is deterministic and unaffected by timestamps
 */

#include "../../include/praxis/PlanHistory.hpp"
#include "../../src/recipe/RecipeTypes.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

using namespace praxis;
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

void test_plan_hash_deterministic_add() {
    std::cout << "[Test] plan_hash_deterministic_add\n";
    
    PlanHistory history1;
    PlanHistory history2;
    
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto plan = make_simple_plan({op1});
    
    // Create same plan in two histories
    history1.create_initial_version("plan1", plan);
    
    // Small delay to ensure different timestamps
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    history2.create_initial_version("plan2", plan);
    
    // Apply same AddStep patch to both
    PlanPatch patch1;
    patch1.plan_id = "plan1";
    patch1.target_version = 1;
    patch1.reason = "Add cube_b";
    
    PatchOperation add_op1;
    add_op1.op_type = PatchOpType::AddStep;
    add_op1.new_step = make_box_op("cube_b", 20, 20, 20);
    patch1.operations.push_back(add_op1);
    
    PlanPatch patch2;
    patch2.plan_id = "plan2";
    patch2.target_version = 1;
    patch2.reason = "Add cube_b";  // Same reason
    
    PatchOperation add_op2;
    add_op2.op_type = PatchOpType::AddStep;
    add_op2.new_step = make_box_op("cube_b", 20, 20, 20);  // Identical operation
    patch2.operations.push_back(add_op2);
    
    auto result1 = history1.apply_patch(patch1);
    
    // Small delay again
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto result2 = history2.apply_patch(patch2);
    
    assert(result1.success);
    assert(result2.success);
    
    // Plan hashes should be identical despite different timestamps
    std::string hash1 = result1.new_plan.resolved_plan.plan_hash;
    std::string hash2 = result2.new_plan.resolved_plan.plan_hash;
    
    assert(!hash1.empty());
    assert(!hash2.empty());
    assert(hash1 == hash2);
    
    std::cout << "  ✓ Plan hash identical despite different timestamps\n";
    std::cout << "  ✓ Hash: " << hash1.substr(0, 16) << "...\n";
}

void test_plan_hash_deterministic_remove() {
    std::cout << "[Test] plan_hash_deterministic_remove\n";
    
    PlanHistory history1;
    PlanHistory history2;
    
    // Create plans with 3 operations
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto op2 = make_box_op("cube_b", 20, 20, 20);
    auto op3 = make_box_op("cube_c", 30, 30, 30);
    auto plan = make_simple_plan({op1, op2, op3});
    
    history1.create_initial_version("plan1", plan);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    history2.create_initial_version("plan2", plan);
    
    // Apply same RemoveStep patch to both (remove middle operation)
    PlanPatch patch1;
    patch1.plan_id = "plan1";
    patch1.target_version = 1;
    patch1.reason = "Remove cube_b";
    
    PatchOperation remove_op1;
    remove_op1.op_type = PatchOpType::RemoveStep;
    remove_op1.step_index = 1;  // Remove middle
    patch1.operations.push_back(remove_op1);
    
    PlanPatch patch2;
    patch2.plan_id = "plan2";
    patch2.target_version = 1;
    patch2.reason = "Remove cube_b";
    
    PatchOperation remove_op2;
    remove_op2.op_type = PatchOpType::RemoveStep;
    remove_op2.step_index = 1;
    patch2.operations.push_back(remove_op2);
    
    auto result1 = history1.apply_patch(patch1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto result2 = history2.apply_patch(patch2);
    
    assert(result1.success);
    assert(result2.success);
    
    // Plan hashes should be identical
    std::string hash1 = result1.new_plan.resolved_plan.plan_hash;
    std::string hash2 = result2.new_plan.resolved_plan.plan_hash;
    
    assert(!hash1.empty());
    assert(!hash2.empty());
    assert(hash1 == hash2);
    
    std::cout << "  ✓ RemoveStep produces deterministic hash\n";
    std::cout << "  ✓ Hash: " << hash1.substr(0, 16) << "...\n";
}

void test_plan_hash_changes_with_content() {
    std::cout << "[Test] plan_hash_changes_with_content\n";
    
    PlanHistory history;
    
    auto op1 = make_box_op("cube_a", 10, 10, 10);
    auto plan = make_simple_plan({op1});
    
    history.create_initial_version("plan", plan);
    
    // Get initial hash
    auto v1 = history.get_version("plan", 1);
    assert(v1.has_value());
    std::string hash1 = v1->resolved_plan.plan_hash;
    
    // Add an operation
    PlanPatch patch_add;
    patch_add.plan_id = "plan";
    patch_add.target_version = 1;
    patch_add.reason = "Add cube_b";
    
    PatchOperation add_op;
    add_op.op_type = PatchOpType::AddStep;
    add_op.new_step = make_box_op("cube_b", 20, 20, 20);
    patch_add.operations.push_back(add_op);
    
    auto result_add = history.apply_patch(patch_add);
    assert(result_add.success);
    std::string hash2 = result_add.new_plan.resolved_plan.plan_hash;
    
    // Remove the operation
    PlanPatch patch_remove;
    patch_remove.plan_id = "plan";
    patch_remove.target_version = 2;
    patch_remove.reason = "Remove cube_b";
    
    PatchOperation remove_op;
    remove_op.op_type = PatchOpType::RemoveStep;
    remove_op.step_index = 1;
    patch_remove.operations.push_back(remove_op);
    
    auto result_remove = history.apply_patch(patch_remove);
    assert(result_remove.success);
    std::string hash3 = result_remove.new_plan.resolved_plan.plan_hash;
    
    // Hash should change when adding operation
    assert(hash1 != hash2);
    
    // Hash should return to original after removing the added operation
    assert(hash1 == hash3);
    
    std::cout << "  ✓ Hash changes with plan content\n";
    std::cout << "  ✓ Hash returns to original after add/remove cycle\n";
}

int main() {
    std::cout << "=== EPIC 1.6.3 Plan Determinism Tests ===\n\n";
    
    test_plan_hash_deterministic_add();
    test_plan_hash_deterministic_remove();
    test_plan_hash_changes_with_content();
    
    std::cout << "\n=== All determinism tests passed ✓ ===\n";
    return 0;
}
