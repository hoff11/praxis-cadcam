// Test op hashing (Sprint 6 Phase 2)
#include "../../src/plan/ExecutionGraph.hpp"
#include "../../src/cache/HashContext.hpp"
#include "../../src/cache/KernelOpHasher.hpp"
#include "../../src/kernel/KernelVersion.hpp"
#include "../../src/recipe/RecipeTypes.hpp"
#include <iostream>
#include <cassert>
#include <stdexcept>

using namespace praxis::recipe;
using namespace praxis::plan;
using namespace praxis::cache;
using namespace praxis::kernel;

// Helper: Create a simple test recipe with dependencies
static Recipe create_test_recipe() {
    Recipe recipe;
    recipe.recipe_version = "0.1";
    recipe.id = "test_hash";
    recipe.units = "mm";
    recipe.kinematics_path = "test.pkm";
    recipe.output = "assembly";
    
    // Node 0: box1 (no deps)
    RecipeNode box1;
    box1.id = "box1";
    box1.op = NodeOp::MakeBox;
    MakeBoxArgs box1_args;
    box1_args.dx = 10.0;
    box1_args.dy = 10.0;
    box1_args.dz = 10.0;
    box1.args = box1_args;
    box1.binding.body = "base";
    recipe.nodes.push_back(box1);
    
    // Node 1: moved (depends on box1)
    RecipeNode moved;
    moved.id = "moved";
    moved.op = NodeOp::Transform;
    TransformArgs moved_args;
    moved_args.input = "box1";
    moved_args.tx = 30.0;
    moved_args.ty = 0.0;
    moved_args.tz = 0.0;
    moved_args.rx = 0.0;
    moved_args.ry = 0.0;
    moved_args.rz = 0.0;
    moved.args = moved_args;
    moved.binding.body = "base";
    recipe.nodes.push_back(moved);
    
    return recipe;
}

// Helper: Create resolved plan from recipe
static ResolvedPlan create_test_resolved_plan(const Recipe& recipe) {
    ResolvedPlan plan;
    plan.output_node = recipe.output;
    plan.engine_version = "test/0.1";
    plan.kernel_version = "test/0.1";
    
    for (const auto& node : recipe.nodes) {
        ResolvedOperation resolved_op;
        resolved_op.node_id = node.id;
        resolved_op.op = node.op;
        
        if (node.op == NodeOp::MakeBox) {
            if (auto* box_args = std::get_if<MakeBoxArgs>(&node.args)) {
                resolved_op.resolved_args["dx"] = std::get<double>(box_args->dx);
                resolved_op.resolved_args["dy"] = std::get<double>(box_args->dy);
                resolved_op.resolved_args["dz"] = std::get<double>(box_args->dz);
            }
        } else if (node.op == NodeOp::Transform) {
            if (auto* transform_args = std::get_if<TransformArgs>(&node.args)) {
                resolved_op.resolved_args["tx"] = std::get<double>(transform_args->tx);
                resolved_op.resolved_args["ty"] = std::get<double>(transform_args->ty);
                resolved_op.resolved_args["tz"] = std::get<double>(transform_args->tz);
                resolved_op.resolved_args["rx"] = std::get<double>(transform_args->rx);
                resolved_op.resolved_args["ry"] = std::get<double>(transform_args->ry);
                resolved_op.resolved_args["rz"] = std::get<double>(transform_args->rz);
            }
        }
        
        plan.operations.push_back(resolved_op);
    }
    
    return plan;
}

// Helper: Create hash context
static HashContext create_hash_context() {
    HashContext ctx;
    
    #ifdef PRAXIS_ENGINE_VERSION
    ctx.engine_version = PRAXIS_ENGINE_VERSION;
    #else
    ctx.engine_version = "dev";
    #endif
    
    ctx.kernel_version = get_kernel_version_string();
    ctx.arg_precision = 6;
    ctx.schema_version = 1;
    ctx.output_contract = "out";
    
    return ctx;
}

int main() {
    std::cout << "========================================\\n";
    std::cout << "Op Hash Tests (Sprint 6 Phase 2)\\n";
    std::cout << "========================================\\n\\n";
    
    // Test 1: Hash determinism
    std::cout << "Test 1: Hash determinism\\n";
    std::cout << "--------------------------------------\\n";
    {
        auto recipe = create_test_recipe();
        auto plan = create_test_resolved_plan(recipe);
        auto ctx = create_hash_context();
        
        auto graph1 = build_graph_from_resolved_plan(recipe, plan);
        graph1 = compute_hashes(graph1, ctx);
        
        auto graph2 = build_graph_from_resolved_plan(recipe, plan);
        graph2 = compute_hashes(graph2, ctx);
        
        assert(graph1.nodes.size() == graph2.nodes.size());
        for (size_t i = 0; i < graph1.nodes.size(); ++i) {
            assert(graph1.nodes[i].op_hash == graph2.nodes[i].op_hash);
            assert(!graph1.nodes[i].op_hash.empty());
            std::cout << "  Node " << i << " (" << graph1.nodes[i].node_id 
                     << "): " << graph1.nodes[i].op_hash.substr(0, 16) << "...\\n";
        }
        
        std::cout << "✓ All hashes are deterministic (byte-identical)\\n\\n";
    }
    
    // Test 2: Arg sensitivity
    std::cout << "Test 2: Arg sensitivity\\n";
    std::cout << "--------------------------------------\\n";
    {
        auto recipe1 = create_test_recipe();
        auto recipe2 = create_test_recipe();
        
        // Modify one arg in box1
        if (auto* box_args = std::get_if<MakeBoxArgs>(&recipe2.nodes[0].args)) {
            const_cast<MakeBoxArgs*>(box_args)->dx = 10.1;  // Changed from 10.0
        }
        
        auto plan1 = create_test_resolved_plan(recipe1);
        auto plan2 = create_test_resolved_plan(recipe2);
        auto ctx = create_hash_context();
        
        auto graph1 = compute_hashes(build_graph_from_resolved_plan(recipe1, plan1), ctx);
        auto graph2 = compute_hashes(build_graph_from_resolved_plan(recipe2, plan2), ctx);
        
        // Node 0 should have different hash
        assert(graph1.nodes[0].op_hash != graph2.nodes[0].op_hash);
        std::cout << "✓ Changing arg dx: 10.0 → 10.1 changed node 0 hash\\n";
        std::cout << "  Old: " << graph1.nodes[0].op_hash.substr(0, 16) << "...\\n";
        std::cout << "  New: " << graph2.nodes[0].op_hash.substr(0, 16) << "...\\n\\n";
    }
    
    // Test 3: Propagation
    std::cout << "Test 3: Dependency propagation\\n";
    std::cout << "--------------------------------------\\n";
    {
        auto recipe1 = create_test_recipe();
        auto recipe2 = create_test_recipe();
        
        // Modify upstream node (box1)
        if (auto* box_args = std::get_if<MakeBoxArgs>(&recipe2.nodes[0].args)) {
            const_cast<MakeBoxArgs*>(box_args)->dy = 10.1;  // Changed from 10.0
        }
        
        auto plan1 = create_test_resolved_plan(recipe1);
        auto plan2 = create_test_resolved_plan(recipe2);
        auto ctx = create_hash_context();
        
        auto graph1 = compute_hashes(build_graph_from_resolved_plan(recipe1, plan1), ctx);
        auto graph2 = compute_hashes(build_graph_from_resolved_plan(recipe2, plan2), ctx);
        
        // Node 0 (box1) should have different hash
        assert(graph1.nodes[0].op_hash != graph2.nodes[0].op_hash);
        std::cout << "✓ Node 0 (box1) hash changed\\n";
        
        // Node 1 (moved, depends on box1) should ALSO have different hash
        assert(graph1.nodes[1].op_hash != graph2.nodes[1].op_hash);
        std::cout << "✓ Node 1 (moved) hash also changed (propagated from dep)\\n\\n";
    }
    
    // Test 4: Version invalidation
    std::cout << "Test 4: Version invalidation\\n";
    std::cout << "--------------------------------------\\n";
    {
        auto recipe = create_test_recipe();
        auto plan = create_test_resolved_plan(recipe);
        
        auto ctx1 = create_hash_context();
        auto ctx2 = create_hash_context();
        ctx2.kernel_version = "different_version";
        
        auto graph1 = compute_hashes(build_graph_from_resolved_plan(recipe, plan), ctx1);
        auto graph2 = compute_hashes(build_graph_from_resolved_plan(recipe, plan), ctx2);
        
        // All nodes should have different hashes
        for (size_t i = 0; i < graph1.nodes.size(); ++i) {
            assert(graph1.nodes[i].op_hash != graph2.nodes[i].op_hash);
        }
        
        std::cout << "✓ Changing kernel_version invalidates all hashes\\n";
        std::cout << "  Version A: " << ctx1.kernel_version << "\\n";
        std::cout << "  Version B: " << ctx2.kernel_version << "\\n\\n";
    }
    
    std::cout << "========================================\\n";
    std::cout << "ALL OP HASH TESTS PASSED!\\n";
    std::cout << "========================================\\n";
    
    return 0;
}
