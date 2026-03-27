// Test ExecutionGraph (Sprint 6 Phase 1)
#include "../../src/plan/ExecutionGraph.hpp"
#include "../../src/recipe/RecipeTypes.hpp"
#include <iostream>
#include <cassert>
#include <stdexcept>

using namespace praxis::recipe;
using namespace praxis::plan;

// Helper: Create a simple test recipe with dependencies
static Recipe create_test_recipe() {
    Recipe recipe;
    recipe.recipe_version = "0.1";
    recipe.id = "test_graph";
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
    
    // Node 1: box2 (no deps)
    RecipeNode box2;
    box2.id = "box2";
    box2.op = NodeOp::MakeBox;
    MakeBoxArgs box2_args;
    box2_args.dx = 20.0;
    box2_args.dy = 20.0;
    box2_args.dz = 20.0;
    box2.args = box2_args;
    box2.binding.body = "base";
    recipe.nodes.push_back(box2);
    
    // Node 2: moved (depends on box1)
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
    
    // Node 3: assembly (depends on moved and box2)
    RecipeNode assembly;
    assembly.id = "assembly";
    assembly.op = NodeOp::Compound;
    CompoundArgs assembly_args;
    assembly_args.node_refs = {"moved", "box2"};
    assembly.args = assembly_args;
    assembly.binding.body = "base";
    recipe.nodes.push_back(assembly);
    
    return recipe;
}

// Helper: Create resolved plan from recipe (simplified for testing)
static ResolvedPlan create_test_resolved_plan(const Recipe& recipe) {
    ResolvedPlan plan;
    plan.output_node = recipe.output;
    plan.engine_version = "test/0.1";
    plan.kernel_version = "test/0.1";
    
    // Create resolved operations (simplified - just copy structure)
    for (const auto& node : recipe.nodes) {
        ResolvedOperation resolved_op;
        resolved_op.node_id = node.id;
        resolved_op.op = node.op;
        
        // Extract numeric args
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
        // Compound has no numeric args
        
        plan.operations.push_back(resolved_op);
    }
    
    return plan;
}

int main() {
    std::cout << "========================================\\n";
    std::cout << "ExecutionGraph Tests (Sprint 6 Phase 1)\\n";
    std::cout << "========================================\\n\\n";
    
    // Test 1: Graph determinism
    std::cout << "Test 1: Graph determinism\\n";
    std::cout << "--------------------------------------\\n";
    {
        auto recipe = create_test_recipe();
        auto plan = create_test_resolved_plan(recipe);
        
        auto graph1 = build_graph_from_resolved_plan(recipe, plan);
        auto graph2 = build_graph_from_resolved_plan(recipe, plan);
        
        std::string json1 = graph1.to_canonical_json();
        std::string json2 = graph2.to_canonical_json();
        
        assert(json1 == json2);
        assert(graph1.nodes.size() == 4);
        
        std::cout << "✓ Graph JSON matches byte-for-byte on repeated builds\\n";
        std::cout << "✓ Graph has " << graph1.nodes.size() << " nodes\\n\\n";
    }
    
    // Test 2: Dependency ordering deterministic
    std::cout << "Test 2: Dependency ordering deterministic\\n";
    std::cout << "--------------------------------------\\n";
    {
        auto recipe = create_test_recipe();
        auto plan = create_test_resolved_plan(recipe);
        
        auto graph = build_graph_from_resolved_plan(recipe, plan);
        
        // Check node 0 (box1) has no deps
        assert(graph.nodes[0].deps.empty());
        std::cout << "✓ Node 0 (box1) has no dependencies\\n";
        
        // Check node 1 (box2) has no deps
        assert(graph.nodes[1].deps.empty());
        std::cout << "✓ Node 1 (box2) has no dependencies\\n";
        
        // Check node 2 (moved) depends on node 0 (box1)
        assert(graph.nodes[2].deps.size() == 1);
        assert(graph.nodes[2].deps[0] == 0);
        std::cout << "✓ Node 2 (moved) depends on node 0\\n";
        
        // Check node 3 (assembly) depends on nodes 1 and 2
        // Deps should be sorted: [1, 2] not [2, 1]
        assert(graph.nodes[3].deps.size() == 2);
        assert(graph.nodes[3].deps[0] == 1);
        assert(graph.nodes[3].deps[1] == 2);
        std::cout << "✓ Node 3 (assembly) depends on nodes [1, 2] (sorted)\\n\\n";
    }
    
    // Test 3: Missing dependency fails loudly
    std::cout << "Test 3: Missing dependency fails loudly\\n";
    std::cout << "--------------------------------------\\n";
    {
        auto recipe = create_test_recipe();
        auto plan = create_test_resolved_plan(recipe);
        
        // Corrupt the recipe: make node reference non-existent node
        recipe.nodes[2] = recipe.nodes[2];  // Keep moved
        if (auto* transform_args = std::get_if<TransformArgs>(&recipe.nodes[2].args)) {
            const_cast<TransformArgs*>(transform_args)->input = "nonexistent";
        }
        
        bool caught_error = false;
        try {
            auto graph = build_graph_from_resolved_plan(recipe, plan);
        } catch (const std::runtime_error& e) {
            caught_error = true;
            std::string err = e.what();
            assert(err.find("non-existent") != std::string::npos);
            std::cout << "✓ Caught expected error: " << e.what() << "\\n\\n";
        }
        
        assert(caught_error);
    }
    
    std::cout << "========================================\\n";
    std::cout << "ALL EXECUTION GRAPH TESTS PASSED!\\n";
    std::cout << "========================================\\n";
    
    return 0;
}
