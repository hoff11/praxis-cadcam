#include "../../src/recipe/RecipeTypes.hpp"
#include "../../src/recipe/RecipeValidator.hpp"
#include "../../src/kinematics/KinematicTypes.hpp"
#include <iostream>
#include <cassert>

using namespace praxis::recipe;
using namespace praxis::kinematics;

int main() {
    std::cout << "========================================\n";
    std::cout << "Recipe-PKM Validation Test Suite\n";
    std::cout << "========================================\n\n";
    
    int total = 0, passed = 0.0;
    
    // Create a test PKM with 3 bodies
    PKM pkm;
    pkm.pkm_version = "0.1";
    pkm.bodies = {"base", "carriage", "spindle"};
    
    // Test 1: Valid recipe with body binding
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        
        RecipeNode node;
        node.id = "box1";
        node.op = NodeOp::MakeBox;
        node.binding.body = "base";
        MakeBoxArgs args;
        args.dx = 100.0; args.dy = 100.0; args.dz = 100.0;
        node.args = args;
        recipe.nodes.push_back(node);
        recipe.output = node.id;
        
        auto result = RecipeValidator::validate_with_pkm(recipe, pkm);
        if (result.valid) {
            std::cout << "Test 1: Valid body binding... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 1: Valid body binding... ✗ FAIL\n";
            for (const auto& err : result.errors) {
                std::cout << "  Error: " << err << "\n";
            }
        }
    }
    
    // Test 2: Invalid body reference should fail
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        
        RecipeNode node;
        node.id = "box1";
        node.op = NodeOp::MakeBox;
        node.binding.body = "nonexistent_body";
        MakeBoxArgs args;
        args.dx = 100.0; args.dy = 100.0; args.dz = 100.0;
        node.args = args;
        recipe.nodes.push_back(node);
        recipe.output = node.id;
        
        auto result = RecipeValidator::validate_with_pkm(recipe, pkm);
        if (!result.valid) {
            std::cout << "Test 2: Invalid body reference rejected... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 2: Invalid body reference rejected... ✗ FAIL (should have been rejected)\n";
        }
    }
    
    // Test 3: Multiple nodes with valid bindings
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        
        RecipeNode node1;
        node1.id = "box1";
        node1.op = NodeOp::MakeBox;
        node1.binding.body = "base";
        MakeBoxArgs args1;
        args1.dx = 100.0; args1.dy = 100.0; args1.dz = 100.0;
        node1.args = args1;
        recipe.nodes.push_back(node1);
        
        RecipeNode node2;
        node2.id = "box2";
        node2.op = NodeOp::MakeBox;
        node2.binding.body = "carriage";
        MakeBoxArgs args2;
        args2.dx = 50.0; args2.dy = 50.0; args2.dz = 50.0;
        node2.args = args2;
        recipe.nodes.push_back(node2);
        recipe.output = node2.id;
        
        auto result = RecipeValidator::validate_with_pkm(recipe, pkm);
        if (result.valid) {
            std::cout << "Test 3: Multiple valid bindings... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 3: Multiple valid bindings... ✗ FAIL\n";
            for (const auto& err : result.errors) {
                std::cout << "  Error: " << err << "\n";
            }
        }
    }
    
    // Test 4: One valid, one invalid should fail
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        
        RecipeNode node1;
        node1.id = "box1";
        node1.op = NodeOp::MakeBox;
        node1.binding.body = "base";
        MakeBoxArgs args1;
        args1.dx = 100.0; args1.dy = 100.0; args1.dz = 100.0;
        node1.args = args1;
        recipe.nodes.push_back(node1);
        
        RecipeNode node2;
        node2.id = "box2";
        node2.op = NodeOp::MakeBox;
        node2.binding.body = "invalid_body";
        MakeBoxArgs args2;
        args2.dx = 50.0; args2.dy = 50.0; args2.dz = 50.0;
        node2.args = args2;
        recipe.nodes.push_back(node2);
        recipe.output = node2.id;
        
        auto result = RecipeValidator::validate_with_pkm(recipe, pkm);
        if (!result.valid) {
            std::cout << "Test 4: Mixed valid/invalid bindings rejected... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 4: Mixed valid/invalid bindings rejected... ✗ FAIL (should have been rejected)\n";
        }
    }
    
    // Test 5: Compound nodes don't need body validation
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        
        RecipeNode node1;
        node1.id = "box1";
        node1.op = NodeOp::MakeBox;
        node1.binding.body = "base";
        MakeBoxArgs args1;
        args1.dx = 100.0; args1.dy = 100.0; args1.dz = 100.0;
        node1.args = args1;
        recipe.nodes.push_back(node1);
        
        RecipeNode compound;
        compound.id = "compound1";
        compound.op = NodeOp::Compound;
        CompoundArgs cargs;
        cargs.node_refs.push_back("box1");
        compound.args = cargs;
        recipe.nodes.push_back(compound);
        recipe.output = compound.id;
        
        auto result = RecipeValidator::validate_with_pkm(recipe, pkm);
        if (result.valid) {
            std::cout << "Test 5: Compound nodes skip body validation... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 5: Compound nodes skip body validation... ✗ FAIL\n";
            for (const auto& err : result.errors) {
                std::cout << "  Error: " << err << "\n";
            }
        }
    }
    
    // Test 6: Node without binding should fail basic validation
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        
        RecipeNode node;
        node.id = "box1";
        node.op = NodeOp::MakeBox;
        node.binding.body = ""; // Empty binding
        MakeBoxArgs args;
        args.dx = 100.0; args.dy = 100.0; args.dz = 100.0;
        node.args = args;
        recipe.nodes.push_back(node);
        recipe.output = node.id;
        
        auto result = RecipeValidator::validate_with_pkm(recipe, pkm);
        if (!result.valid) {
            std::cout << "Test 6: Missing binding rejected... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 6: Missing binding rejected... ✗ FAIL (should have been rejected)\n";
        }
    }
    
    std::cout << "\n========================================\n";
    std::cout << "Summary:\n";
    std::cout << "  Total: " << total << "\n";
    std::cout << "  Passed: " << passed << " ✓\n";
    std::cout << "  Failed: " << (total - passed) << " ✗\n";
    std::cout << "========================================\n";
    
    if (passed == total) {
        std::cout << "✅ All recipe-PKM validation tests passed\n";
        return 0;
    } else {
        std::cout << "❌ Some tests failed\n";
        return 1;
    }
}
