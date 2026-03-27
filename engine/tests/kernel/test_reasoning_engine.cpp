#include "../../src/reasoning/ReasoningEngine.hpp"
#include <iostream>
#include <cassert>

using namespace praxis::reasoning;
using namespace praxis::kinematics;

int main() {
    std::cout << "========================================\n";
    std::cout << "Reasoning Engine Test Suite\n";
    std::cout << "========================================\n\n";
    
    int total = 0, passed = 0;
    
    // Test 1: Travel X exceeds typical envelope
    {
        total++;
        ReasoningEngine engine;
        
        ResolvedRecipe recipe;
        recipe.params["travel_x"] = 1500.0;  // Exceeds 1200mm threshold
        
        PKM pkm;
        
        auto notes = engine.evaluate(recipe, pkm);
        
        bool found_travel_warning = false;
        for (const auto& note : notes) {
            if (note.code == "TRAVEL_X_LARGE") {
                found_travel_warning = true;
                assert(note.level == ReasoningLevel::Warning);
                assert(note.source == "travel_x");
                assert(note.actual_value == 1500.0);
                assert(note.threshold == 1200.0);
            }
        }
        
        if (found_travel_warning) {
            std::cout << "Test 1: Travel X warning triggers correctly... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 1: Travel X warning triggers correctly... ✗ FAIL\n";
        }
    }
    
    // Test 2: Travel Y exceeds envelope
    {
        total++;
        ReasoningEngine engine;
        
        ResolvedRecipe recipe;
        recipe.params["travel_y"] = 900.0;  // Exceeds 800mm threshold
        
        PKM pkm;
        
        auto notes = engine.evaluate(recipe, pkm);
        
        bool found = false;
        for (const auto& note : notes) {
            if (note.code == "TRAVEL_Y_LARGE") {
                found = true;
                assert(note.actual_value == 900.0);
                assert(note.threshold == 800.0);
            }
        }
        
        if (found) {
            std::cout << "Test 2: Travel Y warning triggers correctly... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 2: Travel Y warning triggers correctly... ✗ FAIL\n";
        }
    }
    
    // Test 3: Travel Z exceeds envelope
    {
        total++;
        ReasoningEngine engine;
        
        ResolvedRecipe recipe;
        recipe.params["travel_z"] = 700.0;  // Exceeds 600mm threshold
        
        PKM pkm;
        
        auto notes = engine.evaluate(recipe, pkm);
        
        bool found = false;
        for (const auto& note : notes) {
            if (note.code == "TRAVEL_Z_LARGE") {
                found = true;
            }
        }
        
        if (found) {
            std::cout << "Test 3: Travel Z warning triggers correctly... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 3: Travel Z warning triggers correctly... ✗ FAIL\n";
        }
    }
    
    // Test 4: Travel within limits produces no warnings
    {
        total++;
        ReasoningEngine engine;
        
        ResolvedRecipe recipe;
        recipe.params["travel_x"] = 800.0;  // Within limits
        recipe.params["travel_y"] = 600.0;
        recipe.params["travel_z"] = 500.0;
        
        PKM pkm;
        
        auto notes = engine.evaluate(recipe, pkm);
        
        bool no_travel_warnings = true;
        for (const auto& note : notes) {
            if (note.code.find("TRAVEL_") != std::string::npos) {
                no_travel_warnings = false;
            }
        }
        
        if (no_travel_warnings) {
            std::cout << "Test 4: No warnings for travel within limits... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 4: No warnings for travel within limits... ✗ FAIL\n";
        }
    }
    
    // Test 5: Unusual table aspect ratio triggers info
    {
        total++;
        ReasoningEngine engine;
        
        ResolvedRecipe recipe;
        recipe.params["table_width"] = 1500.0;
        recipe.params["table_depth"] = 400.0;  // Ratio = 3.75 (exceeds 3.0)
        
        PKM pkm;
        
        auto notes = engine.evaluate(recipe, pkm);
        
        bool found = false;
        for (const auto& note : notes) {
            if (note.code == "TABLE_ASPECT_UNUSUAL") {
                found = true;
                assert(note.level == ReasoningLevel::Info);
            }
        }
        
        if (found) {
            std::cout << "Test 5: Unusual table aspect ratio triggers info... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 5: Unusual table aspect ratio triggers info... ✗ FAIL\n";
        }
    }
    
    // Test 6: Spindle clearance risk warning
    {
        total++;
        ReasoningEngine engine;
        
        ResolvedRecipe recipe;
        recipe.params["travel_z"] = 300.0;
        recipe.params["spindle_length"] = 800.0;  // Clearance ratio = 0.375 (< 0.5)
        
        PKM pkm;
        
        auto notes = engine.evaluate(recipe, pkm);
        
        bool found = false;
        for (const auto& note : notes) {
            if (note.code == "SPINDLE_CLEARANCE_RISK") {
                found = true;
                assert(note.level == ReasoningLevel::Warning);
            }
        }
        
        if (found) {
            std::cout << "Test 6: Spindle clearance risk triggers warning... ✓ PASS\n";
            passed++;
        } else {
            std::cout << "Test 6: Spindle clearance risk triggers warning... ✗ FAIL\n";
        }
    }
    
    // Test 7: Rule registry is populated
    {
        total++;
        ReasoningEngine engine;
        
        const auto& rules = engine.get_rules();
        
        if (rules.size() >= 3) {
            std::cout << "Test 7: Rule registry contains expected rules... ✓ PASS\n";
            std::cout << "  Registered rules: " << rules.size() << "\n";
            passed++;
        } else {
            std::cout << "Test 7: Rule registry contains expected rules... ✗ FAIL\n";
        }
    }
    
    std::cout << "\n========================================\n";
    std::cout << "Summary:\n";
    std::cout << "  Total: " << total << "\n";
    std::cout << "  Passed: " << passed << " ✓\n";
    std::cout << "  Failed: " << (total - passed) << " ✗\n";
    std::cout << "========================================\n";
    
    if (passed == total) {
        std::cout << "✅ All reasoning engine tests passed\n";
        return 0;
    } else {
        std::cout << "❌ Some tests failed\n";
        return 1;
    }
}
