#include "../../src/recipe/RecipeTypes.hpp"
#include "../../src/intents/BuildFromRecipe.cpp"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace praxis::recipe;

// Test counters
static int total_tests = 0;
static int passed_tests = 0;

// Test helper
void test(const std::string& name, bool condition) {
    total_tests++;
    if (condition) {
        std::cout << "Test " << total_tests << ": " << name << "... ✓ PASS" << std::endl;
        passed_tests++;
    } else {
        std::cout << "Test " << total_tests << ": " << name << "... ✗ FAIL" << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Recipe Composition Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Get recipe base path (assuming tests run from build directory)
    std::string recipe_base = "../../recipes/test/";
    
    // Test 1: Simple include works
    {
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(recipe_base + "simple_include_test.json", ctx);
        test("Simple include loads successfully", result.success);
        test("Simple include merges params", result.recipe.params.size() == 3);
        test("Simple include merges nodes", result.recipe.nodes.size() == 1);
        test("Simple include node has correct ID", 
             result.success && result.recipe.nodes[0].id == "base_plate");
    }
    
    // Test 2: Param override applies
    {
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(recipe_base + "override_test.json", ctx);
        test("Param override loads successfully", result.success);
        test("Param override applies to included param", 
             result.success && result.recipe.params["plate_width"].default_value == 1500.0);
    }
    
    // Test 3: Multiple nodes merge (included first, then local)
    {
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(recipe_base + "add_node_test.json", ctx);
        test("Multi-node recipe loads successfully", result.success);
        test("Multi-node has correct count", result.success && result.recipe.nodes.size() == 2);
        test("Multi-node: included node comes first", 
             result.success && result.recipe.nodes[0].id == "base_plate");
        test("Multi-node: local node comes second", 
             result.success && result.recipe.nodes[1].id == "column");
    }
    
    // Test 4: Circular include detection
    {
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(recipe_base + "circular_a.json", ctx);
        test("Circular include is detected", !result.success);
        test("Circular include error mentions 'Circular include'", 
             !result.success && result.error_message.find("Circular include") != std::string::npos);
    }
    
    // Test 5: Invalid param override detection
    {
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(recipe_base + "bad_override.json", ctx);
        test("Invalid param override is detected", !result.success);
        test("Invalid param override error mentions 'Unknown parameter'", 
             !result.success && result.error_message.find("Unknown parameter") != std::string::npos);
    }
    
    // Test 6: Node ID collision detection
    {
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(recipe_base + "collision_test.json", ctx);
        test("Node ID collision is detected", !result.success);
        test("Node ID collision error mentions 'Duplicate node'", 
             !result.success && result.error_message.find("Duplicate node") != std::string::npos);
    }
    
    // Test 7: Included recipe cannot have kinematics
    {
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(recipe_base + "include_kinematics_test.json", ctx);
        test("Included recipe with kinematics is rejected", !result.success);
        test("Kinematics error mentions 'cannot specify'", 
             !result.success && result.error_message.find("cannot specify") != std::string::npos);
    }
    
    // Test 8: Units mismatch detection (if units differ)
    {
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(recipe_base + "units_mismatch_test.json", ctx);
        test("Units mismatch is detected", !result.success);
        test("Units mismatch error mentions 'units'", 
             !result.success && result.error_message.find("units") != std::string::npos);
    }
    
    // Test 9: Source provenance populated
    {
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(recipe_base + "add_node_test.json", ctx);
        test("Provenance: recipe loads successfully", result.success);
        test("Provenance: source info populated on included node",
             result.success && !result.recipe.nodes[0].source.recipe_path_display.empty());
        test("Provenance: included node path contains recipe name",
             result.success && result.recipe.nodes[0].source.recipe_path_display.find("base_plate.json") != std::string::npos);
        test("Provenance: included node has operation index",
             result.success && result.recipe.nodes[0].source.operation_index == 0);
        test("Provenance: local node has source info",
             result.success && result.recipe.nodes.size() > 1 && 
             !result.recipe.nodes[1].source.recipe_path_display.empty());
    }
    
    // Test 26: Namespace isolation works
    {
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(recipe_base + "namespace_test.json", ctx);
        if (!result.success) {
            std::cout << "DEBUG: Namespace test error: " << result.error_message << std::endl;
        }
        test("Namespace loads successfully", result.success);
        test("Namespace: qualified params merged", 
             result.success && result.recipe.params.count("spindle.diameter") > 0);
        test("Namespace: no flat merge of params",
             result.success && result.recipe.params.count("diameter") > 0 &&  // base recipe has diameter
             result.recipe.params.count("height") == 0);  // included "height" NOT flat-merged
        test("Namespace: namespace tracked in source",
             result.success && result.recipe.nodes.size() > 0 &&
             result.recipe.nodes[0].source.namespace_alias == "spindle");
    }
    
    // Test 30: Reserved namespace rejected
    {
        // Create a test that tries to use reserved namespace
        std::string test_recipe = recipe_base + "../../scratch/reserved_ns_test.json";
        std::ofstream f(test_recipe);
        f << R"({
  "recipe_version": "0.1",
  "id": "reserved_test",
  "units": "mm",
  "params": {},
  "nodes": [],
  "includes": [
    {
      "recipe_path": "../../../recipes/test/base_plate.json",
      "namespace_alias": "params"
    }
  ],
  "kinematics_path": "../../../recipes/vmc/haas_vf2.pkm",
  "output": "base"
})";
        f.close();
        
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(test_recipe, ctx);
        test("Reserved namespace rejected", !result.success);
        test("Reserved namespace error message", 
             !result.success && result.error_message.find("reserved") != std::string::npos);
        
        std::filesystem::remove(test_recipe);
    }
    
    // Test 32: Invalid namespace regex rejected
    {
        std::string test_recipe = recipe_base + "../../scratch/invalid_ns_test.json";
        std::ofstream f(test_recipe);
        f << R"({
  "recipe_version": "0.1",
  "id": "invalid_test",
  "units": "mm",
  "params": {},
  "nodes": [],
  "includes": [
    {
      "recipe_path": "../../../recipes/test/base_plate.json",
      "namespace_alias": "123invalid"
    }
  ],
  "kinematics_path": "../../../recipes/vmc/haas_vf2.pkm",
  "output": "base"
})";
        f.close();
        
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(test_recipe, ctx);
        test("Invalid namespace regex rejected", !result.success);
     test("Invalid namespace error message", !result.success);
        
        std::filesystem::remove(test_recipe);
    }
    
    // Test 34: Duplicate namespace rejected
    {
        std::string test_recipe = recipe_base + "../../scratch/duplicate_ns_test.json";
        std::ofstream f(test_recipe);
        f << R"({
  "recipe_version": "0.1",
  "id": "duplicate_test",
  "units": "mm",
  "params": {},
  "nodes": [],
  "includes": [
    {
      "recipe_path": "../../../recipes/test/base_plate.json",
      "namespace_alias": "my_ns"
    },
    {
               "recipe_path": "../../../recipes/test/base_plate.json",
      "namespace_alias": "my_ns"
    }
  ],
  "kinematics_path": "../../../recipes/vmc/haas_vf2.pkm",
  "output": "base"
})";
        f.close();
        
        praxis::IncludeContext ctx;
        auto result = praxis::load_recipe_with_includes(test_recipe, ctx);
        test("Duplicate namespace rejected", !result.success);
     test("Duplicate namespace error message", !result.success);
        
        std::filesystem::remove(test_recipe);
    }
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << passed_tests << "/" << total_tests << " tests passed" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return (passed_tests == total_tests) ? 0 : 1;
}
