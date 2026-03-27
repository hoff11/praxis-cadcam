#include "../../src/recipe/RecipeExecutor.hpp"
#include "../../src/recipe/RecipeTypes.hpp"
#include "../../src/kinematics/KinematicTypes.hpp"
#include <iostream>
#include <filesystem>
#include <cassert>

using namespace praxis::recipe;
using namespace praxis::kinematics;

int main() {
    std::cout << "========================================\n";
    std::cout << "Cache Behavior Verification Test\n";
    std::cout << "========================================\n\n";
    
    // Set unique cache directory for test isolation
    setenv("PRAXIS_CACHE_DIR", "./test_cache_dir", 1);
    
    // Clean up any previous test artifacts
    std::filesystem::remove_all("./test_cache_dir");
    std::filesystem::remove_all("./test_cache_out1");
    std::filesystem::remove_all("./test_cache_out2");
    std::filesystem::remove_all("./test_cache_out3");
    
    // Create test PKM
    PKM pkm;
    pkm.pkm_version = "0.1";
    pkm.bodies = {"table"};
    
    Frame frame;
    frame.name = "world";
    frame.origin = {0, 0, 0};
    frame.x_axis = {1, 0, 0};
    frame.y_axis = {0, 1, 0};
    frame.z_axis = {0, 0, 1};
    pkm.frames.push_back(frame);
    
    // Create simple box recipe
    Recipe recipe;
    recipe.recipe_version = "0.1";
    recipe.kinematics_path = "test.pkm";
    recipe.units = "mm";
    
    // Add make_box node
    RecipeNode node;
    node.id = "box";
    node.op = NodeOp::MakeBox;
    MakeBoxArgs args;
    args.dx = 100.0;
    args.dy = 50.0;
    args.dz = 25.0;
    node.args = args;
    node.binding.body = "table";
    recipe.nodes.push_back(node);
    recipe.output = "box";
    
    std::map<std::string, std::string> cli_params;
    std::filesystem::create_directories("test_cache_out1");
    std::filesystem::create_directories("test_cache_out2");
    
    std::cout << "Test 1: First execution (cache miss)\n";
    std::cout << "--------------------------------------\n";
    auto result1 = RecipeExecutor::execute(recipe, pkm, cli_params, "test_cache_out1");
    
    if (!result1.success) {
        std::cerr << "ERROR: First execution failed: " << result1.error_message << "\n";
        return 1;
    }
    
    std::cout << "First execution: SUCCESS\n";
    std::cout << "  kernel_ops executed: " << result1.kernel_ops.size() << "\n";
    std::cout << "  plan_hash: " << result1.resolved_plan.plan_hash << "\n";
    std::cout << "  cache_hit: " << result1.cache.hit << "\n";
    std::cout << "  step_file: " << result1.step_file_path << "\n\n";
    
    if (result1.cache.hit) {
        std::cerr << "ERROR: First execution should be cache miss!\n";
        return 1;
    }
    
    if (result1.kernel_ops.empty()) {
        std::cerr << "ERROR: First execution should have kernel ops!\n";
        return 1;
    }
    
    std::cout << "Test 2: Second execution with same recipe (cache hit)\n";
    std::cout << "------------------------------------------------------\n";
    auto result2 = RecipeExecutor::execute(recipe, pkm, cli_params, "test_cache_out2");
    
    if (!result2.success) {
        std::cerr << "ERROR: Second execution failed: " << result2.error_message << "\n";
        return 1;
    }
    
    std::cout << "Second execution: SUCCESS\n";
    std::cout << "  kernel_ops executed: " << result2.kernel_ops.size() << "\n";
    std::cout << "  plan_hash: " << result2.resolved_plan.plan_hash << "\n";
    std::cout << "  cache_hit: " << result2.cache.hit << "\n";
    std::cout << "  step_file: " << result2.step_file_path << "\n\n";
    
    if (!result2.cache.hit) {
        std::cerr << "ERROR: Second execution should be cache hit!\n";
        return 1;
    }
    
    if (!result2.kernel_ops.empty()) {
        std::cerr << "ERROR: Second execution should have no kernel ops (cached)!\n";
        return 1;
    }
    
    if (result1.resolved_plan.plan_hash != result2.resolved_plan.plan_hash) {
        std::cerr << "ERROR: Plan hashes should match!\n";
        return 1;
    }
    
    std::cout << "Test 3: Modified recipe (cache miss, different hash)\n";
    std::cout << "-----------------------------------------------------\n";
    // Modify the args for make_box
    MakeBoxArgs* box_args = std::get_if<MakeBoxArgs>(&recipe.nodes[0].args);
    if (box_args) {
        box_args->dx = 101.0;  // Change parameter
    }
    std::filesystem::create_directories("test_cache_out3");
    
    auto result3 = RecipeExecutor::execute(recipe, pkm, cli_params, "test_cache_out3");
    
    if (!result3.success) {
        std::cerr << "ERROR: Third execution failed: " << result3.error_message << "\n";
        return 1;
    }
    
    std::cout << "Third execution: SUCCESS\n";
    std::cout << "  kernel_ops executed: " << result3.kernel_ops.size() << "\n";
    std::cout << "  plan_hash: " << result3.resolved_plan.plan_hash << "\n";
    std::cout << "  cache_hit: " << result3.cache.hit << "\n";
    std::cout << "  step_file: " << result3.step_file_path << "\n\n";
    
    if (result3.cache.hit) {
        std::cerr << "ERROR: Third execution should be cache miss (param changed)!\n";
        return 1;
    }
    
    if (result3.kernel_ops.empty()) {
        std::cerr << "ERROR: Third execution should have kernel ops!\n";
        return 1;
    }
    
    if (result1.resolved_plan.plan_hash == result3.resolved_plan.plan_hash) {
        std::cerr << "ERROR: Plan hashes should differ after param change!\n";
        return 1;
    }
    
    std::cout << "========================================\n";
    std::cout << "ALL CACHE TESTS PASSED!\n";
    std::cout << "========================================\n";
    
    return 0;
}
