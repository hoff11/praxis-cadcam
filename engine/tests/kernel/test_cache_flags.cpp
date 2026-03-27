// Test cache control flags (Sprint 5 Phase 6)
#include "../../src/recipe/RecipeExecutor.hpp"
#include "../../src/recipe/RecipeTypes.hpp"
#include "../../src/kinematics/KinematicTypes.hpp"
#include "../../src/cache/ExecutionCache.hpp"
#include <iostream>
#include <filesystem>
#include <cassert>
#include <cstdlib>

namespace fs = std::filesystem;
using namespace praxis;

int main() {
    std::cout << "========================================\n";
    std::cout << "Cache Control Flags Test (Sprint 5 Phase 6)\n";
    std::cout << "========================================\n\n";
    
    // Set cache dir for test isolation
    #ifdef _WIN32
        _putenv("PRAXIS_CACHE_DIR=./test_cache_flags_dir");
    #else
        setenv("PRAXIS_CACHE_DIR", "./test_cache_flags_dir", 1);
    #endif
    
    // Clean up any previous test artifacts
    fs::remove_all("./test_cache_flags_dir");
    fs::remove_all("./test_cache_flags_out1");
    fs::remove_all("./test_cache_flags_out2");
    fs::remove_all("./test_cache_flags_out3");
    fs::remove_all("./test_cache_flags_out4");
    fs::remove_all("./test_cache_flags_out5");
    
    // Create simple test recipe
    recipe::Recipe recipe;
    recipe.recipe_version = "0.1";
    recipe.id = "test_flags";
    recipe.kinematics_path = "dummy.pkm";
    recipe.units = "mm";
    recipe.output = "box1";
    
    recipe::RecipeNode box_node;
    box_node.id = "box1";
    box_node.op = recipe::NodeOp::MakeBox;
    recipe::MakeBoxArgs box_args;
    box_args.dx = 10.0;
    box_args.dy = 10.0;
    box_args.dz = 10.0;
    box_node.args = box_args;
    box_node.binding.body = "dummy";
    recipe.nodes.push_back(box_node);
    
    // Create minimal PKM
    kinematics::PKM pkm;
    pkm.pkm_version = "0.1";
    pkm.bodies = {"dummy"};
    
    kinematics::Frame frame;
    frame.name = "world";
    frame.origin = {0, 0, 0};
    frame.x_axis = {1, 0, 0};
    frame.y_axis = {0, 1, 0};
    frame.z_axis = {0, 0, 1};
    pkm.frames.push_back(frame);
    
    std::map<std::string, std::string> cli_params;
    
    // Test 1: Normal execution (should cache)
    std::cout << "Test 1: Normal execution (builds cache)\n";
    std::cout << "--------------------------------------\n";
    fs::create_directories("test_cache_flags_out1");
    auto result1 = recipe::RecipeExecutor::execute(recipe, pkm, cli_params, "test_cache_flags_out1");
    assert(result1.success);
    assert(!result1.cache.hit);
    assert(result1.kernel_ops.size() > 0);
    std::cout << "First execution: SUCCESS\n";
    std::cout << "  kernel_ops executed: " << result1.kernel_ops.size() << "\n";
    std::cout << "  cache_hit: " << result1.cache.hit << "\n\n";
    
    // Test 2: Second execution (should hit cache)
    std::cout << "Test 2: Second execution (cache hit)\n";
    std::cout << "--------------------------------------\n";
    fs::create_directories("test_cache_flags_out2");
    auto result2 = recipe::RecipeExecutor::execute(recipe, pkm, cli_params, "test_cache_flags_out2");
    assert(result2.success);
    assert(result2.cache.hit);
    assert(result2.kernel_ops.empty());
    std::cout << "Second execution: SUCCESS\n";
    std::cout << "  kernel_ops executed: " << result2.kernel_ops.size() << "\n";
    std::cout << "  cache_hit: " << result2.cache.hit << "\n\n";
    
    // Test 3: With --no-cache flag (should skip cache and execute)
    std::cout << "Test 3: With --no-cache (forces execution)\n";
    std::cout << "--------------------------------------\n";
    fs::create_directories("test_cache_flags_out3");
    auto result3 = recipe::RecipeExecutor::execute(recipe, pkm, cli_params, "test_cache_flags_out3", 
                                                    {}, true, false);  // no_cache=true
    assert(result3.success);
    assert(!result3.cache.hit);
    assert(result3.cache.reason == "Cache disabled via --no-cache flag");
    assert(result3.kernel_ops.size() > 0);
    std::cout << "Third execution: SUCCESS\n";
    std::cout << "  kernel_ops executed: " << result3.kernel_ops.size() << "\n";
    std::cout << "  cache_hit: " << result3.cache.hit << "\n";
    std::cout << "  cache_reason: " << result3.cache.reason << "\n\n";
    
    // Test 4: With --clear-cache flag (should clear cache, then execute)
    std::cout << "Test 4: With --clear-cache (clears and rebuilds)\n";
    std::cout << "--------------------------------------\n";
    
    // Verify cache exists before clear
    cache::ExecutionCache cache_check;
    bool cache_existed = fs::exists(cache_check.get_cache_root());
    std::cout << "Cache exists before clear: " << (cache_existed ? "yes" : "no") << "\n";
    
    fs::create_directories("test_cache_flags_out4");
    auto result4 = recipe::RecipeExecutor::execute(recipe, pkm, cli_params, "test_cache_flags_out4",
                                                    {}, false, true);  // clear_cache=true
    assert(result4.success);
    assert(!result4.cache.hit);  // Should miss since cache was cleared
    assert(result4.kernel_ops.size() > 0);
    std::cout << "Fourth execution: SUCCESS\n";
    std::cout << "  kernel_ops executed: " << result4.kernel_ops.size() << "\n";
    std::cout << "  cache_hit: " << result4.cache.hit << "\n\n";
    
    // Test 5: After clear, next run should hit cache again
    std::cout << "Test 5: After clear, cache rebuilt (cache hit)\n";
    std::cout << "--------------------------------------\n";
    fs::create_directories("test_cache_flags_out5");
    auto result5 = recipe::RecipeExecutor::execute(recipe, pkm, cli_params, "test_cache_flags_out5");
    assert(result5.success);
    assert(result5.cache.hit);
    assert(result5.kernel_ops.empty());
    std::cout << "Fifth execution: SUCCESS\n";
    std::cout << "  kernel_ops executed: " << result5.kernel_ops.size() << "\n";
    std::cout << "  cache_hit: " << result5.cache.hit << "\n\n";
    
    std::cout << "========================================\n";
    std::cout << "ALL CACHE FLAG TESTS PASSED!\n";
    std::cout << "========================================\n";
    
    return 0;
}
