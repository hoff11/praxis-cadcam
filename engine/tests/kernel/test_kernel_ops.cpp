// Layer 1 Kernel API Tests
// Tests for deterministic, regression-safe kernel operations

#include "../../src/kernel/KernelOps.hpp"
#include "../../src/kernel/StepIO.hpp"
#include <iostream>
#include <cmath>
#include <cassert>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

using namespace praxis::kernel;

// Test utilities
static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

void test_start(const std::string& name) {
    std::cout << "Test " << (++test_count) << ": " << name << "... ";
}

void test_pass() {
    std::cout << "✓ PASS\n";
    test_passed++;
}

void test_fail(const std::string& message) {
    std::cout << "✗ FAIL: " << message << "\n";
    test_failed++;
}

// Helper: check doubles are approximately equal
bool approx_equal(double a, double b, double tolerance = 0.001) {
    return std::abs(a - b) <= tolerance;
}

// Test 1: Box creation validates
void test_box_creation() {
    test_start("Box creation produces valid shape");
    
    KernelOps::reset_metrics();
    auto result = KernelOps::make_box(100, 200, 300);
    
    if (!result.success) {
        test_fail("make_box failed: " + result.error_message);
        return;
    }
    
    if (result.shape.is_null()) {
        test_fail("Shape is null");
        return;
    }
    
    if (!KernelOps::validate(result.shape)) {
        test_fail("Box is invalid");
        return;
    }
    
    test_pass();
}

// Test 2: Box bounding box matches dimensions
void test_box_dimensions() {
    test_start("Box bounding box matches dimensions");
    
    const double dx = 100.0, dy = 200.0, dz = 300.0;
    auto result = KernelOps::make_box(dx, dy, dz);
    
    if (!result.success) {
        test_fail("make_box failed");
        return;
    }
    
    auto bbox = KernelOps::get_bounding_box(result.shape);
    
    if (!approx_equal(bbox.dx(), dx)) {
        test_fail("X dimension mismatch: expected " + std::to_string(dx) + 
                  ", got " + std::to_string(bbox.dx()));
        return;
    }
    
    if (!approx_equal(bbox.dy(), dy)) {
        test_fail("Y dimension mismatch: expected " + std::to_string(dy) + 
                  ", got " + std::to_string(bbox.dy()));
        return;
    }
    
    if (!approx_equal(bbox.dz(), dz)) {
        test_fail("Z dimension mismatch: expected " + std::to_string(dz) + 
                  ", got " + std::to_string(bbox.dz()));
        return;
    }
    
    test_pass();
}

// Test 3: Transform preserves validity
void test_transform() {
    test_start("Transform preserves validity");
    
    auto box_result = KernelOps::make_box(50, 50, 50);
    if (!box_result.success) {
        test_fail("make_box failed");
        return;
    }
    
    auto transform_result = KernelOps::transform(box_result.shape, 100, 200, 300);
    
    if (!transform_result.success) {
        test_fail("transform failed: " + transform_result.error_message);
        return;
    }
    
    if (!KernelOps::validate(transform_result.shape)) {
        test_fail("Transformed shape is invalid");
        return;
    }
    
    // Check that bounding box is translated
    auto bbox = KernelOps::get_bounding_box(transform_result.shape);
    
    if (!approx_equal(bbox.xmin, 100.0, 0.1) || 
        !approx_equal(bbox.ymin, 200.0, 0.1) || 
        !approx_equal(bbox.zmin, 300.0, 0.1)) {
        test_fail("Translation incorrect");
        return;
    }
    
    test_pass();
}

// Test 4: Compound creation validates
void test_compound_creation() {
    test_start("Compound creation validates");
    
    std::vector<ShapeHandle> shapes;
    shapes.push_back(KernelOps::make_box(50, 50, 50).shape);
    shapes.push_back(KernelOps::make_box(30, 30, 30).shape);
    shapes.push_back(KernelOps::make_box(20, 20, 20).shape);
    
    auto result = KernelOps::make_compound(shapes);
    
    if (!result.success) {
        test_fail("make_compound failed: " + result.error_message);
        return;
    }
    
    if (!KernelOps::validate(result.shape)) {
        test_fail("Compound is invalid");
        return;
    }
    
    test_pass();
}

// Test 5: STEP round-trip preserves validity
void test_step_roundtrip() {
    test_start("STEP round-trip preserves validity");
    
    // Create a box
    auto box_result = KernelOps::make_box(100, 150, 200);
    if (!box_result.success) {
        test_fail("make_box failed");
        return;
    }
    
    bool original_valid = KernelOps::validate(box_result.shape);
    
    // Write to STEP
    std::string temp_path = "/tmp/test_kernel_roundtrip.step";
    auto write_result = StepIO::write_step(box_result.shape, temp_path);
    
    if (!write_result.success) {
        test_fail("write_step failed: " + write_result.error_message);
        return;
    }
    
    // Read back
    auto read_result = StepIO::read_step(temp_path);
    
    if (!read_result.success) {
        test_fail("read_step failed: " + read_result.error_message);
        return;
    }
    
    bool roundtrip_valid = KernelOps::validate(ShapeHandle(read_result.shape));
    
    if (original_valid && !roundtrip_valid) {
        test_fail("Validity not preserved after round-trip");
        return;
    }
    
    // Check dimensions preserved (approximately)
    auto original_bbox = KernelOps::get_bounding_box(box_result.shape);
    auto roundtrip_bbox = KernelOps::get_bounding_box(ShapeHandle(read_result.shape));
    
    if (!approx_equal(original_bbox.dx(), roundtrip_bbox.dx(), 0.1) ||
        !approx_equal(original_bbox.dy(), roundtrip_bbox.dy(), 0.1) ||
        !approx_equal(original_bbox.dz(), roundtrip_bbox.dz(), 0.1)) {
        test_fail("Dimensions not preserved after round-trip");
        return;
    }
    
    // Cleanup
    fs::remove(temp_path);
    
    test_pass();
}

// Test 6: Heal preserves or improves validity
void test_heal() {
    test_start("Heal preserves or improves validity");
    
    // Create a valid box
    auto box_result = KernelOps::make_box(100, 100, 100);
    if (!box_result.success) {
        test_fail("make_box failed");
        return;
    }
    
    bool valid_before = KernelOps::validate(box_result.shape);
    
    // Heal it
    auto heal_result = KernelOps::heal(box_result.shape);
    
    if (!heal_result.success) {
        test_fail("heal failed: " + heal_result.error_message);
        return;
    }
    
    bool valid_after = KernelOps::validate(heal_result.shape);
    
    // If it was valid before, it must remain valid (or be invalid due to edge cases)
    // The key rule: heal doesn't make things worse
    if (valid_before && !valid_after) {
        test_fail("Valid shape became invalid after healing");
        return;
    }
    
    test_pass();
}

// Test 7: Metrics are tracked correctly
void test_metrics() {
    test_start("Metrics tracked correctly");
    
    KernelOps::reset_metrics();
    
    // Perform operations
    auto box1 = KernelOps::make_box(50, 50, 50);
    auto box2 = KernelOps::make_box(30, 30, 30);
    auto transformed = KernelOps::transform(box1.shape, 10, 20, 30);
    std::vector<ShapeHandle> shapes = {box1.shape, box2.shape};
    auto compound = KernelOps::make_compound(shapes);
    bool valid = KernelOps::validate(compound.shape);
    auto bbox = KernelOps::get_bounding_box(compound.shape);
    
    // Check metrics
    auto metrics = KernelOps::get_metrics();
    
    if (metrics["make_box"] != 2) {
        test_fail("make_box count incorrect: expected 2, got " + 
                  std::to_string(metrics["make_box"]));
        return;
    }
    
    if (metrics["transform"] != 1) {
        test_fail("transform count incorrect: expected 1, got " + 
                  std::to_string(metrics["transform"]));
        return;
    }
    
    if (metrics["make_compound"] != 1) {
        test_fail("make_compound count incorrect: expected 1, got " + 
                  std::to_string(metrics["make_compound"]));
        return;
    }
    
    if (metrics["validate"] != 1) {
        test_fail("validate count incorrect: expected 1, got " + 
                  std::to_string(metrics["validate"]));
        return;
    }
    
    if (metrics["get_bounding_box"] != 1) {
        test_fail("get_bounding_box count incorrect: expected 1, got " + 
                  std::to_string(metrics["get_bounding_box"]));
        return;
    }
    
    test_pass();
}

// Test 8: Error handling for invalid inputs
void test_error_handling() {
    test_start("Error handling for invalid inputs");
    
    // Test negative box dimensions
    auto bad_box = KernelOps::make_box(-10, 50, 50);
    if (bad_box.success) {
        test_fail("make_box should fail with negative dimensions");
        return;
    }
    
    // Test transform on null shape
    ShapeHandle null_shape;
    auto bad_transform = KernelOps::transform(null_shape, 10, 10, 10);
    if (bad_transform.success) {
        test_fail("transform should fail on null shape");
        return;
    }
    
    // Test compound with empty list
    std::vector<ShapeHandle> empty_shapes;
    auto bad_compound = KernelOps::make_compound(empty_shapes);
    if (bad_compound.success) {
        test_fail("make_compound should fail with empty shape list");
        return;
    }
    
    // Test read of non-existent file
    auto bad_read = StepIO::read_step("/nonexistent/path/file.step");
    if (bad_read.success) {
        test_fail("read_step should fail on non-existent file");
        return;
    }
    
    test_pass();
}

// Test 9: Operations are traced correctly
void test_operation_tracing() {
    test_start("Operations traced correctly");
    
    auto box_result = KernelOps::make_box(100, 200, 300);
    
    if (box_result.operations.empty()) {
        test_fail("No operations traced");
        return;
    }
    
    // Check that operation string contains expected values
    bool found_make_box = false;
    for (const auto& op : box_result.operations) {
        if (op.find("make_box") != std::string::npos &&
            op.find("100") != std::string::npos &&
            op.find("200") != std::string::npos &&
            op.find("300") != std::string::npos) {
            found_make_box = true;
            break;
        }
    }
    
    if (!found_make_box) {
        test_fail("make_box operation not properly traced");
        return;
    }
    
    test_pass();
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Layer 1 Kernel API Test Suite\n";
    std::cout << "========================================\n\n";
    
    // Run all tests
    test_box_creation();
    test_box_dimensions();
    test_transform();
    test_compound_creation();
    test_step_roundtrip();
    test_heal();
    test_metrics();
    test_error_handling();
    test_operation_tracing();
    
    // Summary
    std::cout << "\n========================================\n";
    std::cout << "Test Summary\n";
    std::cout << "========================================\n";
    std::cout << "Total:  " << test_count << "\n";
    std::cout << "Passed: " << test_passed << " ✓\n";
    std::cout << "Failed: " << test_failed << " ✗\n";
    std::cout << "========================================\n";
    
    if (test_failed == 0) {
        std::cout << "✅ All tests passed\n";
        return 0;
    } else {
        std::cout << "❌ Some tests failed\n";
        return 1;
    }
}
