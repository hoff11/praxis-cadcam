/**
 * test_selector_parsing.cpp
 * 
 * Sprint 7 - Test Suite A1: Selector Grammar Parsing
 * 
 * Acceptance:
 * - Valid grammar parses successfully
 * - Invalid syntax → InvalidSelector
 * - Illegal targets per mode → ContractViolation
 */

#include "../../../include/Selector.hpp"
#include <cassert>
#include <iostream>

using namespace praxis::selector;

void test_valid_selector_parsing() {
    std::cout << "Testing valid selector parsing..." << std::endl;
    
    SelectorExecutor executor(nullptr);
    
    // Test: product:body?index=0!one
    {
        auto parsed = executor.parse("product:body?index=0!one");
        assert(parsed.has_value());
        assert(parsed->mode == SelectionMode::Product);
        assert(parsed->target == "body");
        assert(parsed->cardinality == Cardinality::One);
        assert(parsed->filters.size() == 1);
        assert(parsed->filters[0].attribute == "index");
        assert(parsed->filters[0].value == "0");
        std::cout << "  ✓ product:body?index=0!one" << std::endl;
    }
    
    // Test: feature:face@body(index=0)?type=planar!one
    {
        auto parsed = executor.parse("feature:face@body(index=0)?type=planar!one");
        assert(parsed.has_value());
        assert(parsed->mode == SelectionMode::Feature);
        assert(parsed->target == "face");
        assert(parsed->cardinality == Cardinality::One);
        assert(parsed->scope.has_value());
        assert(parsed->scope->target == "body");
        assert(parsed->filters.size() == 1);
        assert(parsed->filters[0].attribute == "type");
        assert(parsed->filters[0].value == "planar");
        std::cout << "  ✓ feature:face@body(index=0)?type=planar!one" << std::endl;
    }
    
    // Test: feature:edge?type=circular!many
    {
        auto parsed = executor.parse("feature:edge?type=circular!many");
        assert(parsed.has_value());
        assert(parsed->mode == SelectionMode::Feature);
        assert(parsed->target == "edge");
        assert(parsed->cardinality == Cardinality::Many);
        std::cout << "  ✓ feature:edge?type=circular!many" << std::endl;
    }
    
    // Test: default cardinality (!one)
    {
        auto parsed = executor.parse("product:body");
        assert(parsed.has_value());
        assert(parsed->cardinality == Cardinality::One);
        std::cout << "  ✓ Default cardinality is !one" << std::endl;
    }
    
    // Test: multiple filters
    {
        auto parsed = executor.parse("feature:face?type=planar&area=max!one");
        assert(parsed.has_value());
        assert(parsed->filters.size() == 2);
        assert(parsed->filters[0].attribute == "type");
        assert(parsed->filters[1].attribute == "area");
        std::cout << "  ✓ Multiple filters with &" << std::endl;
    }
    
    std::cout << "[✓] Valid selector parsing tests passed" << std::endl;
}

void test_invalid_syntax() {
    std::cout << "Testing invalid syntax rejection..." << std::endl;
    
    SelectorExecutor executor(nullptr);
    
    // Empty string
    {
        auto parsed = executor.parse("");
        assert(!parsed.has_value());
        std::cout << "  ✓ Empty string rejected" << std::endl;
    }
    
    // Missing mode (no colon)
    {
        auto parsed = executor.parse("face");
        assert(!parsed.has_value());
        std::cout << "  ✓ Missing mode rejected" << std::endl;
    }
    
    // Invalid cardinality
    {
        auto parsed = executor.parse("product:body!two");
        assert(!parsed.has_value());
        std::cout << "  ✓ Invalid cardinality rejected" << std::endl;
    }
    
    // Invalid mode
    {
        auto parsed = executor.parse("invalid:face");
        assert(!parsed.has_value());
        std::cout << "  ✓ Invalid mode rejected" << std::endl;
    }
    
    // Malformed scope (missing parentheses)
    {
        auto parsed = executor.parse("feature:face@body");
        assert(!parsed.has_value());
        std::cout << "  ✓ Malformed scope rejected" << std::endl;
    }
    
    std::cout << "[✓] Invalid syntax tests passed" << std::endl;
}

void test_illegal_targets() {
    std::cout << "Testing illegal target rejection (contract violations)..." << std::endl;
    
    SelectorExecutor executor(nullptr);
    
    // product:face (illegal: face not in product mode)
    {
        auto parsed = executor.parse("product:face");
        assert(!parsed.has_value());
        std::cout << "  ✓ product:face rejected (contract violation)" << std::endl;
    }
    
    // feature:body (illegal: body not in feature mode)
    {
        auto parsed = executor.parse("feature:body");
        assert(!parsed.has_value());
        std::cout << "  ✓ feature:body rejected (contract violation)" << std::endl;
    }
    
    // feature:sketch (illegal: sketch is forbidden)
    {
        auto parsed = executor.parse("feature:sketch");
        assert(!parsed.has_value());
        std::cout << "  ✓ feature:sketch rejected (forbidden)" << std::endl;
    }
    
    // feature:extrude (illegal: feature tree nodes forbidden)
    {
        auto parsed = executor.parse("feature:extrude");
        assert(!parsed.has_value());
        std::cout << "  ✓ feature:extrude rejected (forbidden)" << std::endl;
    }
    
    std::cout << "[✓] Illegal target tests passed" << std::endl;
}

int main() {
    std::cout << "\n=== Sprint 7 - Selector Parsing Tests ===" << std::endl;
    
    try {
        test_valid_selector_parsing();
        test_invalid_syntax();
        test_illegal_targets();
        
        std::cout << "\n✅ All parsing tests PASSED!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
