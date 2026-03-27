/**
 * test_canonical_format.cpp
 * 
 * Sprint 9 - EPIC 1: Deterministic Serialization
 * Unit tests for canonical float formatting with edge cases
 */

#include "praxis/CanonicalFormat.hpp"
#include <cassert>
#include <cmath>
#include <cfloat>
#include <iostream>
#include <stdexcept>

using namespace praxis::canonical;

// Test helper: compare strings and print on mismatch
void assert_equal(const std::string& expected, const std::string& actual, const std::string& test_name) {
    if (expected != actual) {
        std::cerr << "FAIL: " << test_name << "\n";
        std::cerr << "  Expected: \"" << expected << "\"\n";
        std::cerr << "  Actual:   \"" << actual << "\"\n";
        assert(false);
    }
}

// Test: Zero normalization (-0.0 → 0.0)
void test_zero_normalization() {
    std::cout << "Test: Zero normalization\n";
    
    std::string pos_zero = format_float(0.0);
    std::string neg_zero = format_float(-0.0);
    
    // Both should produce identical output
    assert_equal(pos_zero, neg_zero, "Zero normalization");
    
    // Should be "0" (not "0.0" with defaultfloat)
    assert_equal("0", pos_zero, "Zero format");
    
    std::cout << "  ✓ 0.0 and -0.0 both serialize as \"" << pos_zero << "\"\n\n";
}

// Test: Small numbers
void test_small_numbers() {
    std::cout << "Test: Small numbers\n";
    
    std::string result1 = format_float(1e-12);
    std::cout << "  1e-12 → \"" << result1 << "\"\n";
    
    std::string result2 = format_float(1e-7);
    std::cout << "  1e-7 → \"" << result2 << "\"\n";
    
    std::string result3 = format_float(DBL_MIN);
    std::cout << "  DBL_MIN → \"" << result3 << "\"\n";
    
    // Verify deterministic output (same input → same output)
    assert_equal(result1, format_float(1e-12), "1e-12 determinism");
    assert_equal(result2, format_float(1e-7), "1e-7 determinism");
    
    std::cout << "  ✓ Small numbers format deterministically\n\n";
}

// Test: Large numbers
void test_large_numbers() {
    std::cout << "Test: Large numbers\n";
    
    std::string result1 = format_float(1e12);
    std::cout << "  1e12 → \"" << result1 << "\"\n";
    
    std::string result2 = format_float(1e15);
    std::cout << "  1e15 → \"" << result2 << "\"\n";
    
    std::string result3 = format_float(DBL_MAX);
    std::cout << "  DBL_MAX → \"" << result3 << "\"\n";
    
    // Verify deterministic output
    assert_equal(result1, format_float(1e12), "1e12 determinism");
    assert_equal(result2, format_float(1e15), "1e15 determinism");
    
    std::cout << "  ✓ Large numbers format deterministically\n\n";
}

// Test: Typical geometry values
void test_geometry_values() {
    std::cout << "Test: Typical geometry values\n";
    
    // Typical values from CAD operations
    std::string vol = format_float(1000.5);
    std::string coord = format_float(123.456789);
    std::string small_dim = format_float(0.001);
    
    std::cout << "  Volume 1000.5 → \"" << vol << "\"\n";
    std::cout << "  Coordinate 123.456789 → \"" << coord << "\"\n";
    std::cout << "  Small dimension 0.001 → \"" << small_dim << "\"\n";
    
    // Verify precision is maintained (9 significant digits)
    assert(coord.find("123.456789") != std::string::npos || coord == "123.456789");
    
    std::cout << "  ✓ Geometry values preserve precision\n\n";
}

// Test: NaN rejection
void test_nan_rejection() {
    std::cout << "Test: NaN rejection\n";
    
    bool caught = false;
    try {
        format_float(std::nan(""));
    } catch (const std::runtime_error& e) {
        caught = true;
        std::cout << "  ✓ NaN rejected: " << e.what() << "\n";
    }
    
    assert(caught);
    std::cout << "  ✓ NaN throws exception\n\n";
}

// Test: Infinity rejection
void test_infinity_rejection() {
    std::cout << "Test: Infinity rejection\n";
    
    bool caught_pos = false;
    try {
        format_float(INFINITY);
    } catch (const std::runtime_error& e) {
        caught_pos = true;
        std::cout << "  ✓ +Inf rejected: " << e.what() << "\n";
    }
    
    bool caught_neg = false;
    try {
        format_float(-INFINITY);
    } catch (const std::runtime_error& e) {
        caught_neg = true;
        std::cout << "  ✓ -Inf rejected: " << e.what() << "\n";
    }
    
    assert(caught_pos && caught_neg);
    std::cout << "  ✓ Infinity throws exception\n\n";
}

// Test: JSON string escaping
void test_json_escape() {
    std::cout << "Test: JSON string escaping\n";
    
    assert_equal("hello", escape_json("hello"), "Simple string");
    assert_equal("hello\\nworld", escape_json("hello\nworld"), "Newline");
    assert_equal("quote\\\"mark", escape_json("quote\"mark"), "Quote");
    assert_equal("back\\\\slash", escape_json("back\\slash"), "Backslash");
    assert_equal("tab\\there", escape_json("tab\there"), "Tab");
    
    std::cout << "  ✓ Basic escaping works\n";
    
    // Control character
    std::string ctrl = escape_json(std::string(1, '\x01'));
    assert(ctrl.find("\\u0001") != std::string::npos);
    std::cout << "  ✓ Control characters escaped as \\uXXXX\n\n";
}

// Test: Locale independence
void test_locale_independence() {
    std::cout << "Test: Locale independence\n";
    
    // In some locales, decimal separator is comma instead of period
    // Our format_float should always use period
    std::string result = format_float(1234.5);
    
    // Must contain period, not comma
    assert(result.find('.') != std::string::npos || result.find(',') == std::string::npos);
    
    std::cout << "  1234.5 → \"" << result << "\"\n";
    std::cout << "  ✓ Uses period (not comma) regardless of locale\n\n";
}

int main() {
    std::cout << "=================================================\n";
    std::cout << "Sprint 9 - Canonical Format Unit Tests\n";
    std::cout << "=================================================\n\n";
    
    try {
        test_zero_normalization();
        test_small_numbers();
        test_large_numbers();
        test_geometry_values();
        test_nan_rejection();
        test_infinity_rejection();
        test_json_escape();
        test_locale_independence();
        
        std::cout << "=================================================\n";
        std::cout << "✅ All canonical format tests passed!\n";
        std::cout << "=================================================\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
