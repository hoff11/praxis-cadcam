#include "../../src/recipe/RecipeTypes.hpp"
#include "../../src/recipe/RecipeValidator.hpp"
#include <iostream>
#include <cassert>

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

// Helper to create minimal valid recipe
Recipe create_valid_recipe() {
    Recipe recipe;
    recipe.recipe_version = "0.1";
    recipe.id = "test_recipe";
    recipe.units = "mm";
    recipe.kinematics_path = "test.pkm";
    recipe.output = "box1";
    
    // Add a simple node with binding
    RecipeNode node;
    node.id = "box1";
    node.op = NodeOp::MakeBox;
    node.args = MakeBoxArgs{100.0, 200.0, 300.0};
    node.binding.body = "base";
    
    recipe.nodes.push_back(node);
    
    return recipe;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Recipe Validator Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Test 1: Valid recipe passes
    {
        Recipe recipe = create_valid_recipe();
        auto result = RecipeValidator::validate(recipe);
        test("Valid recipe passes validation", result.valid);
    }
    
    // Test 2: Missing recipe version rejected
    {
        Recipe recipe = create_valid_recipe();
        recipe.recipe_version = "";
        auto result = RecipeValidator::validate(recipe);
        test("Missing recipe version rejected", !result.valid);
    }
    
    // Test 3: Wrong recipe version rejected
    {
        Recipe recipe = create_valid_recipe();
        recipe.recipe_version = "999.0";
        auto result = RecipeValidator::validate(recipe);
        test("Wrong recipe version rejected", !result.valid);
    }
    
    // Test 4: Missing kinematics path rejected
    {
        Recipe recipe = create_valid_recipe();
        recipe.kinematics_path = "";
        auto result = RecipeValidator::validate(recipe);
        test("Missing kinematics path rejected", !result.valid);
    }
    
    // Test 5: Duplicate node IDs rejected
    {
        Recipe recipe = create_valid_recipe();
        RecipeNode node2 = recipe.nodes[0];
        recipe.nodes.push_back(node2);  // Duplicate ID
        auto result = RecipeValidator::validate(recipe);
        test("Duplicate node IDs rejected", !result.valid);
    }
    
    // Test 6: Node without body binding rejected
    {
        Recipe recipe = create_valid_recipe();
        recipe.nodes[0].binding.body = "";  // Remove binding
        auto result = RecipeValidator::validate(recipe);
        test("Node without body binding rejected", !result.valid);
    }
    
    // Test 7: Invalid compound node reference rejected
    {
        Recipe recipe = create_valid_recipe();
        
        RecipeNode compound;
        compound.id = "assembly";
        compound.op = NodeOp::Compound;
        compound.args = CompoundArgs{{"nonexistent_node"}};
        
        recipe.nodes.push_back(compound);
        
        auto result = RecipeValidator::validate(recipe);
        test("Invalid compound node reference rejected", !result.valid);
    }
    
    // Test 8: Parameter with min > max rejected
    {
        Recipe recipe = create_valid_recipe();
        
        ParamDef param;
        param.name = "bad_param";
        param.min_value = 100;
        param.max_value = 50;  // min > max
        param.default_value = 75;
        
        recipe.params["bad_param"] = param;
        
        auto result = RecipeValidator::validate(recipe);
        test("Parameter with min > max rejected", !result.valid);
    }
    
    // Test 9: Parameter default outside range rejected
    {
        Recipe recipe = create_valid_recipe();
        
        ParamDef param;
        param.name = "bad_default";
        param.min_value = 50;
        param.max_value = 100;
        param.default_value = 150;  // Outside range
        
        recipe.params["bad_default"] = param;
        
        auto result = RecipeValidator::validate(recipe);
        test("Parameter default outside range rejected", !result.valid);
    }
    
    // Test 10: Empty derived expression rejected
    {
        Recipe recipe = create_valid_recipe();
        
        Expression expr;
        expr.formula = "";  // Empty
        
        recipe.derived["empty_expr"] = expr;
        
        auto result = RecipeValidator::validate(recipe);
        test("Empty derived expression rejected", !result.valid);
    }
    
    // Test 11: Compound nodes don't need bindings
    {
        Recipe recipe = create_valid_recipe();
        
        RecipeNode compound;
        compound.id = "assembly";
        compound.op = NodeOp::Compound;
        compound.args = CompoundArgs{{"box1"}};
        // No binding - should be OK for compound
        
        recipe.nodes.push_back(compound);
        
        auto result = RecipeValidator::validate(recipe);
        test("Compound nodes don't need bindings", result.valid);
    }
    
    // Test 12: Valid recipe with parameters and derived values
    {
        Recipe recipe = create_valid_recipe();
        
        ParamDef width;
        width.name = "width";
        width.min_value = 100;
        width.max_value = 1000;
        width.default_value = 500;
        recipe.params["width"] = width;
        
        Expression total_width;
        total_width.formula = "width + 50";
        recipe.derived["total_width"] = total_width;
        
        auto result = RecipeValidator::validate(recipe);
        test("Valid recipe with parameters and derived values", result.valid);
    }
    
    // Print summary
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  Total: " << total_tests << std::endl;
    std::cout << "  Passed: " << passed_tests << " ✓" << std::endl;
    std::cout << "  Failed: " << (total_tests - passed_tests) << " ✗" << std::endl;
    std::cout << "========================================" << std::endl;
    
    if (passed_tests == total_tests) {
        std::cout << "✅ All recipe validator tests passed" << std::endl;
        return 0;
    } else {
        std::cout << "❌ Some tests failed" << std::endl;
        return 1;
    }
}
