#include "../../src/recipe/RecipeTypes.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <map>

using namespace praxis::recipe;

int main() {
    std::cout << "========================================\n";
    std::cout << "Expression Evaluator Test Suite\n";
    std::cout << "========================================\n\n";
    
    int total = 0, passed = 0;
    
    std::map<std::string, double> values = {
        {"width", 100.0},
        {"height", 200.0},
        {"depth", 300.0},
        {"offset", 50.0}
    };
    
    // Test 1: Simple variable lookup
    {
        total++;
        Expression expr;
        expr.formula = "width";
        try {
            double result = expr.evaluate(values);
            if (std::abs(result - 100.0) < 0.001) {
                std::cout << "Test 1: Variable lookup... ✓ PASS\n";
                passed++;
            } else {
                std::cout << "Test 1: Variable lookup... ✗ FAIL (expected 100, got " << result << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Test 1: Variable lookup... ✗ FAIL (" << e.what() << ")\n";
        }
    }
    
    // Test 2: Simple constant
    {
        total++;
        Expression expr;
        expr.formula = "42.5";
        try {
            double result = expr.evaluate(values);
            if (std::abs(result - 42.5) < 0.001) {
                std::cout << "Test 2: Constant value... ✓ PASS\n";
                passed++;
            } else {
                std::cout << "Test 2: Constant value... ✗ FAIL (expected 42.5, got " << result << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Test 2: Constant value... ✗ FAIL (" << e.what() << ")\n";
        }
    }
    
    // Test 3: Addition
    {
        total++;
        Expression expr;
        expr.formula = "width + offset";
        try {
            double result = expr.evaluate(values);
            if (std::abs(result - 150.0) < 0.001) {
                std::cout << "Test 3: Addition... ✓ PASS\n";
                passed++;
            } else {
                std::cout << "Test 3: Addition... ✗ FAIL (expected 150, got " << result << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Test 3: Addition... ✗ FAIL (" << e.what() << ")\n";
        }
    }
    
    // Test 4: Subtraction
    {
        total++;
        Expression expr;
        expr.formula = "depth - offset";
        try {
            double result = expr.evaluate(values);
            if (std::abs(result - 250.0) < 0.001) {
                std::cout << "Test 4: Subtraction... ✓ PASS\n";
                passed++;
            } else {
                std::cout << "Test 4: Subtraction... ✗ FAIL (expected 250, got " << result << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Test 4: Subtraction... ✗ FAIL (" << e.what() << ")\n";
        }
    }
    
    // Test 5: Multiplication
    {
        total++;
        Expression expr;
        expr.formula = "width * 2";
        try {
            double result = expr.evaluate(values);
            if (std::abs(result - 200.0) < 0.001) {
                std::cout << "Test 5: Multiplication... ✓ PASS\n";
                passed++;
            } else {
                std::cout << "Test 5: Multiplication... ✗ FAIL (expected 200, got " << result << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Test 5: Multiplication... ✗ FAIL (" << e.what() << ")\n";
        }
    }
    
    // Test 6: Division
    {
        total++;
        Expression expr;
        expr.formula = "height / 4";
        try {
            double result = expr.evaluate(values);
            if (std::abs(result - 50.0) < 0.001) {
                std::cout << "Test 6: Division... ✓ PASS\n";
                passed++;
            } else {
                std::cout << "Test 6: Division... ✗ FAIL (expected 50, got " << result << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Test 6: Division... ✗ FAIL (" << e.what() << ")\n";
        }
    }
    
    // Test 7: Parentheses
    {
        total++;
        Expression expr;
        expr.formula = "(width + offset) * 2";
        try {
            double result = expr.evaluate(values);
            if (std::abs(result - 300.0) < 0.001) {
                std::cout << "Test 7: Parentheses... ✓ PASS\n";
                passed++;
            } else {
                std::cout << "Test 7: Parentheses... ✗ FAIL (expected 300, got " << result << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Test 7: Parentheses... ✗ FAIL (" << e.what() << ")\n";
        }
    }
    
    // Test 8: Operator precedence
    {
        total++;
        Expression expr;
        expr.formula = "width + offset * 2";
        try {
            double result = expr.evaluate(values);
            if (std::abs(result - 200.0) < 0.001) {
                std::cout << "Test 8: Operator precedence... ✓ PASS\n";
                passed++;
            } else {
                std::cout << "Test 8: Operator precedence... ✗ FAIL (expected 200, got " << result << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Test 8: Operator precedence... ✗ FAIL (" << e.what() << ")\n";
        }
    }
    
    // Test 9: Complex expression
    {
        total++;
        Expression expr;
        expr.formula = "(width + height) / 2 + offset";
        try {
            double result = expr.evaluate(values);
            if (std::abs(result - 200.0) < 0.001) {
                std::cout << "Test 9: Complex expression... ✓ PASS\n";
                passed++;
            } else {
                std::cout << "Test 9: Complex expression... ✗ FAIL (expected 200, got " << result << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Test 9: Complex expression... ✗ FAIL (" << e.what() << ")\n";
        }
    }
    
    // Test 10: Negative numbers
    {
        total++;
        Expression expr;
        expr.formula = "-width + height";
        try {
            double result = expr.evaluate(values);
            if (std::abs(result - 100.0) < 0.001) {
                std::cout << "Test 10: Negative numbers... ✓ PASS\n";
                passed++;
            } else {
                std::cout << "Test 10: Negative numbers... ✗ FAIL (expected 100, got " << result << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Test 10: Negative numbers... ✗ FAIL (" << e.what() << ")\n";
        }
    }
    
    // Test 11: Unknown variable should fail
    {
        total++;
        Expression expr;
        expr.formula = "unknown_var + 10";
        try {
            double result = expr.evaluate(values);
            std::cout << "Test 11: Unknown variable error... ✗ FAIL (should have thrown)\n";
        } catch (const std::exception& e) {
            std::cout << "Test 11: Unknown variable error... ✓ PASS\n";
            passed++;
        }
    }
    
    // Test 12: Division by zero should fail
    {
        total++;
        Expression expr;
        expr.formula = "width / 0";
        try {
            double result = expr.evaluate(values);
            std::cout << "Test 12: Division by zero error... ✗ FAIL (should have thrown)\n";
        } catch (const std::exception& e) {
            std::cout << "Test 12: Division by zero error... ✓ PASS\n";
            passed++;
        }
    }
    
    std::cout << "\n========================================\n";
    std::cout << "Summary:\n";
    std::cout << "  Total: " << total << "\n";
    std::cout << "  Passed: " << passed << " ✓\n";
    std::cout << "  Failed: " << (total - passed) << " ✗\n";
    std::cout << "========================================\n";
    
    if (passed == total) {
        std::cout << "✅ All expression evaluator tests passed\n";
        return 0;
    } else {
        std::cout << "❌ Some tests failed\n";
        return 1;
    }
}
