// PKM Validator Tests
// Tests for PKM validation rules from sprint2.md

#include "../../src/kinematics/KinematicTypes.hpp"
#include "../../src/kinematics/KinematicValidator.hpp"
#include <iostream>
#include <cassert>

using namespace praxis::kinematics;

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

// Helper to create a minimal valid PKM
PKM create_valid_pkm() {
    PKM pkm;
    pkm.pkm_version = "0.1";  // Required
    pkm.machine = "test_machine";
    pkm.units = "mm";
    pkm.bodies = {"base", "table"};
    
    Joint joint;
    joint.id = "X";
    joint.type = JointType::Prismatic;
    joint.parent_body = "base";
    joint.child_body = "table";
    joint.axis_origin = Vec3(0, 0, 0);
    joint.axis_direction = Vec3(1, 0, 0);  // Unit vector
    joint.limits = {0, 1000};
    
    pkm.joints.push_back(joint);
    pkm.chain = {"X"};
    
    return pkm;
}

void test_valid_pkm() {
    test_start("Valid PKM passes validation");
    
    PKM pkm = create_valid_pkm();
    auto result = KinematicValidator::validate(pkm);
    
    if (!result.valid) {
        test_fail("Valid PKM rejected: " + (result.errors.empty() ? "unknown" : result.errors[0]));
        return;
    }
    
    test_pass();
}

void test_missing_version() {
    test_start("Missing PKM version rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.pkm_version = "";  // Remove version
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Missing version not detected");
        return;
    }
    
    test_pass();
}

void test_wrong_version() {
    test_start("Wrong PKM version rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.pkm_version = "9.9";  // Unknown version must be rejected
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Wrong version not detected");
        return;
    }
    
    test_pass();
}

void test_version_02_accepted() {
    test_start("PKM version 0.2 accepted");

    PKM pkm = create_valid_pkm();
    pkm.pkm_version = "0.2";

    auto result = KinematicValidator::validate(pkm);

    if (!result.valid) {
        test_fail("PKM version 0.2 should be accepted");
        return;
    }

    test_pass();
}

void test_revolute_limits_sanity() {
    test_start("Unreasonable revolute limits rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.joints[0].type = JointType::Revolute;
    pkm.joints[0].limits = {-1000, 1000};  // ±1000° is absurd
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Absurd revolute limits not detected");
        return;
    }
    
    test_pass();
}

void test_prismatic_limits_sanity() {
    test_start("Unreasonable prismatic limits rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.joints[0].limits = {0, 200000};  // 200 meters is absurd
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Absurd prismatic limits not detected");
        return;
    }
    
    test_pass();
}

void test_duplicate_body_ids() {
    test_start("Duplicate body IDs rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.bodies.push_back("base");  // Duplicate
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Duplicate body IDs not detected");
        return;
    }
    
    test_pass();
}

void test_duplicate_joint_ids() {
    test_start("Duplicate joint IDs rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.joints.push_back(pkm.joints[0]);  // Duplicate joint
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Duplicate joint IDs not detected");
        return;
    }
    
    test_pass();
}

void test_unknown_parent_body() {
    test_start("Unknown parent body rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.joints[0].parent_body = "nonexistent";
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Unknown parent body not detected");
        return;
    }
    
    test_pass();
}

void test_self_referencing_joint() {
    test_start("Self-referencing joint rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.joints[0].child_body = pkm.joints[0].parent_body;
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Self-referencing joint not detected");
        return;
    }
    
    test_pass();
}

void test_zero_magnitude_axis() {
    test_start("Zero magnitude axis rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.joints[0].axis_direction = Vec3(0, 0, 0);
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Zero magnitude axis not detected");
        return;
    }
    
    test_pass();
}

void test_non_unit_axis_warning() {
    test_start("Non-unit axis generates warning");
    
    PKM pkm = create_valid_pkm();
    pkm.joints[0].axis_direction = Vec3(2, 0, 0);  // Magnitude 2
    
    auto result = KinematicValidator::validate(pkm);
    
    if (!result.valid) {
        test_fail("Non-unit axis incorrectly rejected");
        return;
    }
    
    if (result.warnings.empty()) {
        test_fail("No warning generated for non-unit axis");
        return;
    }
    
    test_pass();
}

void test_invalid_limits() {
    test_start("Invalid limits rejected (min > max)");
    
    PKM pkm = create_valid_pkm();
    pkm.joints[0].limits = {1000, 0};  // min > max
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Invalid limits not detected");
        return;
    }
    
    test_pass();
}

void test_chain_unknown_joint() {
    test_start("Chain with unknown joint rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.chain.push_back("Y");  // Y doesn't exist
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Unknown joint in chain not detected");
        return;
    }
    
    test_pass();
}

void test_chain_duplicate() {
    test_start("Chain with duplicate joints rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.chain.push_back("X");  // Duplicate
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Duplicate in chain not detected");
        return;
    }
    
    test_pass();
}

void test_empty_chain_with_joints() {
    test_start("Empty chain with joints rejected");
    
    PKM pkm = create_valid_pkm();
    pkm.chain.clear();
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Empty chain not detected");
        return;
    }
    
    test_pass();
}

void test_cycle_detection() {
    test_start("Cyclic kinematic chain rejected");
    
    PKM pkm;
    pkm.machine = "cyclic";
    pkm.units = "mm";
    pkm.bodies = {"A", "B", "C"};
    
    // Create a cycle: A -> B -> C -> A
    Joint j1;
    j1.id = "J1";
    j1.type = JointType::Prismatic;
    j1.parent_body = "A";
    j1.child_body = "B";
    j1.axis_origin = Vec3(0, 0, 0);
    j1.axis_direction = Vec3(1, 0, 0);
    j1.limits = {0, 100};
    
    Joint j2 = j1;
    j2.id = "J2";
    j2.parent_body = "B";
    j2.child_body = "C";
    
    Joint j3 = j1;
    j3.id = "J3";
    j3.parent_body = "C";
    j3.child_body = "A";  // Creates cycle
    
    pkm.joints = {j1, j2, j3};
    pkm.chain = {"J1", "J2", "J3"};
    
    auto result = KinematicValidator::validate(pkm);
    
    if (result.valid) {
        test_fail("Cycle not detected");
        return;
    }
    
    test_pass();
}

int main() {
    std::cout << "========================================\n";
    std::cout << "PKM Validator Test Suite\n";
    std::cout << "========================================\n\n";
    
    test_valid_pkm();
    test_missing_version();
    test_wrong_version();
    test_version_02_accepted();
    test_revolute_limits_sanity();
    test_prismatic_limits_sanity();
    test_duplicate_body_ids();
    test_duplicate_joint_ids();
    test_unknown_parent_body();
    test_self_referencing_joint();
    test_zero_magnitude_axis();
    test_non_unit_axis_warning();
    test_invalid_limits();
    test_chain_unknown_joint();
    test_chain_duplicate();
    test_empty_chain_with_joints();
    test_cycle_detection();
    
    std::cout << "\n========================================\n";
    std::cout << "Test Summary\n";
    std::cout << "========================================\n";
    std::cout << "Total:  " << test_count << "\n";
    std::cout << "Passed: " << test_passed << " ✓\n";
    std::cout << "Failed: " << test_failed << " ✗\n";
    std::cout << "========================================\n";
    
    if (test_failed == 0) {
        std::cout << "✅ All PKM validation tests passed\n";
        return 0;
    } else {
        std::cout << "❌ Some tests failed\n";
        return 1;
    }
}
