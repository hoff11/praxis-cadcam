// Integration test for op-level caching (Sprint 6 Phase 3)
#include "../../src/recipe/RecipeExecutor.hpp"
#include "../../src/recipe/RecipeTypes.hpp"
#include "../../src/kinematics/KinematicTypes.hpp"
#include "../../src/cache/OpCache.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace praxis::recipe;
using namespace praxis::kinematics;
using namespace praxis::cache;
namespace fs = std::filesystem;

// Helper: Create minimal PKM for testing
static PKM create_test_pkm() {
    PKM pkm;
    pkm.pkm_version = "0.1";
    pkm.machine = "test_machine";
    pkm.units = "mm";
    pkm.bodies = {"base"};
    
    Frame world_frame;
    world_frame.name = "world";
    world_frame.origin = {0, 0, 0};
    world_frame.x_axis = {1, 0, 0};
    world_frame.y_axis = {0, 1, 0};
    world_frame.z_axis = {0, 0, 1};
    pkm.frames.push_back(world_frame);
    
    return pkm;
}

// Helper: Create simple test recipe with 4 nodes
static Recipe create_test_recipe(double size_value, const std::string& recipe_id = "test_op_cache") {
    Recipe recipe;
    recipe.recipe_version = "0.1";
    recipe.id = recipe_id;  // Use passed recipe_id to differentiate plans
    recipe.units = "mm";
    recipe.kinematics_path = "test.pkm";
    recipe.output = "result";
    
    // Param: size
    ParamDef size_param;
    size_param.name = "size";
    size_param.default_value = size_value;
    size_param.min_value = 1.0;
    size_param.max_value = 1000.0;
    recipe.params["size"] = size_param;
    
    // Node 0: box1 (depends on param "size")
    RecipeNode box1;
    box1.id = "box1";
    box1.op = NodeOp::MakeBox;
    MakeBoxArgs box1_args;
    // Use expression to reference parameter
    Expression size_expr;
    size_expr.formula = "size";
    box1_args.dx = size_expr;
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
    moved_args.tx = 20.0;
    moved_args.ty = 0.0;
    moved_args.tz = 0.0;
    moved_args.rx = 0.0;
    moved_args.ry = 0.0;
    moved_args.rz = 0.0;
    moved.args = moved_args;
    moved.binding.body = "base";
    recipe.nodes.push_back(moved);
    
    // Node 2: box2 (independent of size param)
    RecipeNode box2;
    box2.id = "box2";
    box2.op = NodeOp::MakeBox;
    MakeBoxArgs box2_args;
    box2_args.dx = 5.0;
    box2_args.dy = 5.0;
    box2_args.dz = 5.0;
    box2.args = box2_args;
    box2.binding.body = "base";
    recipe.nodes.push_back(box2);
    
    // Node 3: result (compound depends on moved and box2)
    RecipeNode result;
    result.id = "result";
    result.op = NodeOp::Compound;
    CompoundArgs result_args;
    result_args.node_refs = {"moved", "box2"};
    result.args = result_args;
    result.binding.body = "base";
    recipe.nodes.push_back(result);
    
    return recipe;
}

int main() {
    std::cout << "\n=== Op Cache Integration Tests ===\n";
    int total = 0, passed = 0;
    
    PKM pkm = create_test_pkm();
    
    // Create test output directory
    fs::path test_output_dir = fs::temp_directory_path() / "praxis_op_cache_integration_test";
    if (fs::exists(test_output_dir)) {
        fs::remove_all(test_output_dir);
    }
    fs::create_directories(test_output_dir);
    
    // Set PRAXIS_CACHE_DIR environment variable to use test-specific cache
    fs::path test_cache_root = test_output_dir / "cache";
    fs::create_directories(test_cache_root);
    
    // Set environment variable (platform-specific)
    #ifdef _WIN32
    _putenv_s("PRAXIS_CACHE_DIR", test_cache_root.string().c_str());
    #else
    setenv("PRAXIS_CACHE_DIR", test_cache_root.string().c_str(), 1);
    #endif
    
    std::map<std::string, std::string> cli_params;
    std::string output_dir = test_output_dir.string();
    
    // Test 1: First run - all nodes execute
    {
        total++;
        std::cout << "\nTest 1: First run executes all 4 nodes...\n";
        
        Recipe recipe = create_test_recipe(10.0, "test1_recipe");
        std::string out_dir = output_dir + "/test1";
        fs::create_directories(out_dir);
        
        auto result = RecipeExecutor::execute(recipe, pkm, cli_params, out_dir);
        
        if (!result.success) {
            std::cout << "  ✗ FAIL: Execution failed: " << result.error_message << "\n";
        } else if (result.op_cache.executed_count != 4) {
            std::cout << "  ✗ FAIL: Expected 4 executed, got " << result.op_cache.executed_count << "\n";
        } else if (result.op_cache.reused_count != 0) {
            std::cout << "  ✗ FAIL: Expected 0 reused, got " << result.op_cache.reused_count << "\n";
        } else if (result.op_cache.cache_hit_rate != 0.0) {
            std::cout << "  ✗ FAIL: Expected 0% cache hit rate, got " << (result.op_cache.cache_hit_rate * 100) << "%\n";
        } else {
            std::cout << "  ✓ PASS: 4 nodes executed, 0 reused, 0% cache hit rate\n";
            passed++;
        }
    }
    
    // Test 2: Plan cache hit (demonstrates multi-level caching)
    {
        total++;
        std::cout << "\nTest 2: Plan cache hit skips execution entirely...\n";
        
        Recipe recipe = create_test_recipe(10.0, "test2_recipe");  // Same params as test1
        std::string out_dir = output_dir + "/test2";
        fs::create_directories(out_dir);
        
        // Same recipe structure and params means plan cache will hit
        auto result = RecipeExecutor::execute(recipe, pkm, cli_params, out_dir);
        
        if (!result.success) {
            std::cout << "  ✗ FAIL: Execution failed: " << result.error_message << "\n";
        } else if (!result.cache.hit) {
            std::cout << "  ✗ FAIL: Expected plan cache hit, got miss\n";
        } else if (result.op_cache.executed_count != 0 || result.op_cache.reused_count != 0) {
            std::cout << "  ✗ FAIL: Plan cache hit should skip all op execution\n";
        } else {
            std::cout << "  ✓ PASS: Plan cache hit, no op execution needed (fastest path)\n";
            passed++;
        }
    }
    
    // Test 3: Upstream parameter change - selective invalidation
    {
        total++;
        std::cout << "\nTest 3: Upstream parameter change invalidates affected nodes...\n";
        
        Recipe recipe = create_test_recipe(15.0, "test3_recipe");  // Changed size param
        std::string out_dir = output_dir + "/test3";
        fs::create_directories(out_dir);
        
        auto result = RecipeExecutor::execute(recipe, pkm, cli_params, out_dir);
        
        if (!result.success) {
            std::cout << "  ✗ FAIL: Execution failed: " << result.error_message << "\n";
        } else if (result.op_cache.executed_count != 3) {
            std::cout << "  ✗ FAIL: Expected 3 executed (box1, moved, result), got " << result.op_cache.executed_count << "\n";
            std::cout << "  Node status:\n";
            for (const auto& node : result.op_cache.node_status) {
                std::cout << "    " << node.node_id << " (" << node.op_type << "): " << node.status 
                          << ", cache_hit=" << (node.cache_hit ? "true" : "false") << "\n";
            }
        } else if (result.op_cache.reused_count != 1) {
            std::cout << "  ✗ FAIL: Expected 1 reused (box2), got " << result.op_cache.reused_count << "\n";
        } else {
            std::cout << "  ✓ PASS: 3 nodes executed (affected by param change), 1 reused (box2 independent)\n";
            passed++;
        }
    }
    
    // Test 4: --no-cache flag disables op caching
    {
        total++;
        std::cout << "\nTest 4: --no-cache flag disables op cache...\n";
        
        Recipe recipe = create_test_recipe(10.0, "test4_recipe");  // Same as cached version
        std::string out_dir = output_dir + "/test4";
        fs::create_directories(out_dir);
        
        auto result = RecipeExecutor::execute(recipe, pkm, cli_params, out_dir, 
                                             {}, true);  // no_cache=true
        
        if (!result.success) {
            std::cout << "  ✗ FAIL: Execution failed: " << result.error_message << "\n";
        } else if (result.op_cache.executed_count != 4) {
            std::cout << "  ✗ FAIL: Expected 4 executed (cache disabled), got " << result.op_cache.executed_count << "\n";
        } else if (result.op_cache.reused_count != 0) {
            std::cout << "  ✗ FAIL: Expected 0 reused (cache disabled), got " << result.op_cache.reused_count << "\n";
        } else {
            std::cout << "  ✓ PASS: All 4 nodes executed (op cache disabled by flag)\n";
            passed++;
        }
    }
    
    // Test 5: Verify node status structure (with execution)
    {
        total++;
        std::cout << "\nTest 5: Node status structure is correctly populated...\n";
        
        Recipe recipe = create_test_recipe(20.0, "test5_recipe");  // Different params to avoid plan cache
        std::string out_dir = output_dir + "/test5";
        fs::create_directories(out_dir);
        
        auto result = RecipeExecutor::execute(recipe, pkm, cli_params, out_dir);
        
        if (!result.success) {
            std::cout << "  ✗ FAIL: Execution failed: " << result.error_message << "\n";
        } else if (result.op_cache.node_status.size() != 4) {
            std::cout << "  ✗ FAIL: Expected 4 node status entries, got " << result.op_cache.node_status.size() << "\n";
        } else {
            bool all_have_hashes = true;
            bool all_have_paths = true;
            for (const auto& node : result.op_cache.node_status) {
                if (node.op_hash.empty()) {
                    all_have_hashes = false;
                    std::cout << "  Node " << node.node_id << " missing op_hash\n";
                }
                if (node.artifact_path.empty()) {
                    all_have_paths = false;
                    std::cout << "  Node " << node.node_id << " missing artifact_path\n";
                }
            }
            
            if (!all_have_hashes || !all_have_paths) {
                std::cout << "  ✗ FAIL: Some nodes missing op_hash or artifact_path\n";
            } else {
                std::cout << "  ✓ PASS: All nodes have op_hash, artifact_path, and status\n";
                passed++;
            }
        }
    }
    
    // Cleanup
    fs::remove_all(test_output_dir);
    
    // Summary
    std::cout << "\n=================================\n";
    std::cout << "Tests passed: " << passed << "/" << total << "\n";
    std::cout << "=================================\n\n";
    
    return (passed == total) ? 0 : 1;
}
