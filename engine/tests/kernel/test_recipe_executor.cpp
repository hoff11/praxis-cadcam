#include "../../src/recipe/RecipeExecutor.hpp"
#include "../../src/recipe/RecipeTypes.hpp"
#include "../../src/kinematics/KinematicTypes.hpp"
#include <iostream>
#include <cassert>

using namespace praxis::recipe;
using namespace praxis::kinematics;

int main() {
    std::cout << "========================================\n";
    std::cout << "Recipe Executor Validation Tests\n";
    std::cout << "========================================\n\n";
    
    int total = 0, passed = 0;
    
    // Create test PKM with bodies and frames
    PKM pkm;
    pkm.pkm_version = "0.1";
    pkm.bodies = {"base", "carriage", "spindle"};
    
    Frame baseFrame;
    baseFrame.name = "world";
    baseFrame.origin = {0, 0, 0};
    baseFrame.x_axis = {1, 0, 0};
    baseFrame.y_axis = {0, 1, 0};
    baseFrame.z_axis = {0, 0, 1};
    pkm.frames.push_back(baseFrame);
    
    Frame carriageFrame;
    carriageFrame.name = "carriage_frame";
    carriageFrame.origin = {0, 0, 100};
    carriageFrame.x_axis = {1, 0, 0};
    carriageFrame.y_axis = {0, 1, 0};
    carriageFrame.z_axis = {0, 0, 1};
    pkm.frames.push_back(carriageFrame);
    
    // Empty CLI params for validation tests
    std::map<std::string, std::string> empty_cli_params;
    
    // Test 1: Valid variant with in-range overrides passes
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        recipe.units = "mm";
        
        // Add parameter with range
        ParamDef param;
        param.name = "size";
        param.min_value = 10.0;
        param.max_value = 200.0;
        param.default_value = 100.0;
        recipe.params["size"] = param;
        
        // Add variant with valid override
        Variant variant;
        variant.name = "small";
        variant.param_overrides["size"] = 50.0;  // Within [10, 200]
        recipe.variants["small"] = variant;
        
        // Add node using parameter
        RecipeNode node;
        node.id = "box1";
        node.op = NodeOp::MakeBox;
        MakeBoxArgs args;
        args.dx = 0.0;  // Will be set by param
        args.dy = 0.0;
        args.dz = 0.0;
        node.args = args;
        node.binding.body = "base";
        recipe.nodes.push_back(node);
        recipe.output = "box1";
        
        // Add variant to cli params
        std::map<std::string, std::string> cli_params;
        cli_params["variant"] = "small";
        
        auto result = RecipeExecutor::validate_only(recipe, pkm, cli_params);
        
        if (result.success && result.error_message.empty()) {
            std::cout << "Test 1: Valid variant with in-range overrides... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 1: Valid variant with in-range overrides... ✗ FAIL\n";
            std::cout << "  Error: " << result.error_message << "\n";
        }
    }
    
    // Test 2: Variant with out-of-range override rejected
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        recipe.units = "mm";
        
        // Add parameter with range
        ParamDef param;
        param.name = "size";
        param.min_value = 10.0;
        param.max_value = 200.0;
        param.default_value = 100.0;
        recipe.params["size"] = param;
        
        // Add variant with OUT OF RANGE override
        Variant variant;
        variant.name = "too_large";
        variant.param_overrides["size"] = 300.0;  // Outside [10, 200] - should fail!
        recipe.variants["too_large"] = variant;
        
        RecipeNode node;
        node.id = "box1";
        node.op = NodeOp::MakeBox;
        MakeBoxArgs args;
        args.dx = 100.0;
        args.dy = 100.0;
        args.dz = 100.0;
        node.args = args;
        node.binding.body = "base";
        recipe.nodes.push_back(node);
        recipe.output = "box1";
        
        std::map<std::string, std::string> cli_params;
        cli_params["variant"] = "too_large";
        
        auto result = RecipeExecutor::validate_only(recipe, pkm, cli_params);
        
        bool has_range_error = false;
        if (!result.success && !result.error_message.empty()) {
            if (result.error_message.find("out of range") != std::string::npos || 
                result.error_message.find("outside allowed range") != std::string::npos) {
                has_range_error = true;
            }
        }
        
        if (!result.success && has_range_error) {
            std::cout << "Test 2: Variant out-of-range override rejected... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 2: Variant out-of-range override rejected... ✗ FAIL (should have been rejected)\n";
            std::cout << "  Error message: " << result.error_message << "\n";
        }
    }
    
    // Test 3: Unknown variant name rejected
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        recipe.units = "mm";
        
        // Add parameter
        ParamDef param;
        param.name = "size";
        param.min_value = 10.0;
        param.max_value = 200.0;
        param.default_value = 100.0;
        recipe.params["size"] = param;
        
        // Add a valid variant
        Variant variant;
        variant.name = "small";
        variant.param_overrides["size"] = 50.0;
        recipe.variants["small"] = variant;
        
        RecipeNode node;
        node.id = "box1";
        node.op = NodeOp::MakeBox;
        MakeBoxArgs args;
        args.dx = 100.0;
        args.dy = 100.0;
        args.dz = 100.0;
        node.args = args;
        node.binding.body = "base";
        recipe.nodes.push_back(node);
        recipe.output = "box1";
        
        std::map<std::string, std::string> cli_params;
        cli_params["variant"] = "nonexistent_variant";
        
        auto result = RecipeExecutor::validate_only(recipe, pkm, cli_params);  // Request unknown variant
        
        bool has_variant_error = false;
        if (!result.success && !result.error_message.empty()) {
            if (result.error_message.find("variant") != std::string::npos && 
                result.error_message.find("nknown") != std::string::npos) {
                has_variant_error = true;
            }
        }
        
        if (!result.success && has_variant_error) {
            std::cout << "Test 3: Unknown variant name rejected... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 3: Unknown variant name rejected... ✗ FAIL (should have been rejected)\n";
            std::cout << "  Error message: " << result.error_message << "\n";
        }
    }
    
    // Test 4: Node with invalid frame binding rejected
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        recipe.units = "mm";
        
        RecipeNode node;
        node.id = "box1";
        node.op = NodeOp::MakeBox;
        MakeBoxArgs args;
        args.dx = 100.0;
        args.dy = 100.0;
        args.dz = 100.0;
        node.args = args;
        node.binding.body = "base";
        node.binding.frame = "nonexistent_frame";  // Invalid frame!
        recipe.nodes.push_back(node);
        recipe.output = "box1";
        
        auto result = RecipeExecutor::validate_only(recipe, pkm, empty_cli_params);
        
        bool has_frame_error = false;
        if (!result.success && !result.error_message.empty()) {
            if (result.error_message.find("frame") != std::string::npos && 
                (result.error_message.find("unknown") != std::string::npos || 
                 result.error_message.find("not found") != std::string::npos || 
                 result.error_message.find("does not exist") != std::string::npos ||
                 result.error_message.find("invalid") != std::string::npos)) {
                has_frame_error = true;
            }
        }
        
        if (!result.success && has_frame_error) {
            std::cout << "Test 4: Invalid frame binding rejected... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 4: Invalid frame binding rejected... ✗ FAIL (should have been rejected)\n";
            std::cout << "  Error message: " << result.error_message << "\n";
        }
    }
    
    // Test 5: Node with valid frame binding passes
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        recipe.units = "mm";
        
        RecipeNode node;
        node.id = "box1";
        node.op = NodeOp::MakeBox;
        MakeBoxArgs args;
        args.dx = 100.0;
        args.dy = 100.0;
        args.dz = 100.0;
        node.args = args;
        node.binding.body = "base";
        node.binding.frame = "world";  // Valid frame from PKM
        recipe.nodes.push_back(node);
        recipe.output = "box1";
        
        auto result = RecipeExecutor::validate_only(recipe, pkm, empty_cli_params);
        
        if (result.success && result.error_message.empty()) {
            std::cout << "Test 5: Valid frame binding passes... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 5: Valid frame binding passes... ✗ FAIL\n";
            std::cout << "  Error: " << result.error_message << "\n";
        }
    }
    
    // Test 6: Node with empty frame binding passes (frame is optional)
    {
        total++;
        Recipe recipe;
        recipe.recipe_version = "0.1";
        recipe.kinematics_path = "test.pkm";
        recipe.units = "mm";
        
        RecipeNode node;
        node.id = "box1";
        node.op = NodeOp::MakeBox;
        MakeBoxArgs args;
        args.dx = 100.0;
        args.dy = 100.0;
        args.dz = 100.0;
        node.args = args;
        node.binding.body = "base";
        node.binding.frame = "";  // Empty is allowed
        recipe.nodes.push_back(node);
        recipe.output = "box1";
        
        auto result = RecipeExecutor::validate_only(recipe, pkm, empty_cli_params);
        
        if (result.success && result.error_message.empty()) {
            std::cout << "Test 6: Empty frame binding passes... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 6: Empty frame binding passes... ✗ FAIL\n";
            std::cout << "  Error: " << result.error_message << "\n";
        }
    }
    
    std::cout << "\n========================================\n";
    std::cout << "Summary:\n";
    std::cout << "  Total: " << total << "\n";
    std::cout << "  Passed: " << passed << " ✓\n";
    std::cout << "  Failed: " << (total - passed) << " ✗\n";
    std::cout << "========================================\n";
    
    if (passed == total) {
        std::cout << "✅ All recipe executor validation tests passed\n";
        return 0;
    } else {
        std::cout << "❌ Some tests failed\n";
        return 1;
    }
}
