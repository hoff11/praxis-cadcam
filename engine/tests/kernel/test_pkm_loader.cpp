#include "../../src/kinematics/KinematicLoader.hpp"
#include "../../src/kinematics/KinematicValidator.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

using namespace praxis::kinematics;

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
    std::cout << "PKM Loader Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Create temp test files
    std::filesystem::create_directories("/tmp/pkm_loader_tests");
    
    // Test 1: Load simple PKM from file
    {
        std::string pkm_content = R"({
  "pkm_version": "0.1",
  "machine": "test_simple",
  "units": "mm",
  "bodies": ["base", "moving"],
  "joints": [],
  "chain": []
})";
        std::ofstream file("/tmp/pkm_loader_tests/simple.pkm");
        file << pkm_content;
        file.close();
        
        auto result = KinematicLoader::load_from_json("/tmp/pkm_loader_tests/simple.pkm");
        test("Load simple PKM from file", result.success && 
             result.pkm.machine == "test_simple" &&
             result.pkm.bodies.size() == 2);
    }
    
    // Test 2: Load PKM with single joint
    {
        std::string pkm_content = R"({
  "pkm_version": "0.1",
  "machine": "test_1joint",
  "units": "mm",
  "bodies": ["base", "link1"],
  "joints": [
    {
      "id": "J1",
      "type": "revolute",
      "parent_body": "base",
      "child_body": "link1",
      "axis_origin": [0, 0, 0],
      "axis_direction": [0, 0, 1],
      "limits": {
        "min": -90,
        "max": 90
      }
    }
  ],
  "chain": ["J1"]
})";
        std::ofstream file("/tmp/pkm_loader_tests/onejoint.pkm");
        file << pkm_content;
        file.close();
        
        auto result = KinematicLoader::load_from_json("/tmp/pkm_loader_tests/onejoint.pkm");
        test("Load PKM with one joint", result.success &&
             result.pkm.joints.size() == 1);
        test("Joint ID parsed correctly", result.pkm.joints.size() > 0 &&
             result.pkm.joints[0].id == "J1");
        test("Joint type parsed correctly", result.pkm.joints.size() > 0 &&
             result.pkm.joints[0].type == JointType::Revolute);
        test("Joint limits parsed correctly", result.pkm.joints.size() > 0 &&
             result.pkm.joints[0].limits.min == -90 &&
             result.pkm.joints[0].limits.max == 90);
    }
    
    // Test 3: Load PKM with multiple joints (VMC-like)
    {
        std::string pkm_content = R"({
  "pkm_version": "0.1",
  "machine": "test_3axis",
  "units": "mm",
  "bodies": ["base", "x_car", "y_car", "z_car"],
  "joints": [
    {
      "id": "X",
      "type": "prismatic",
      "parent_body": "base",
      "child_body": "x_car",
      "axis_origin": [0, 0, 0],
      "axis_direction": [1, 0, 0],
      "limits": {
        "min": 0,
        "max": 800
      }
    },
    {
      "id": "Y",
      "type": "prismatic",
      "parent_body": "x_car",
      "child_body": "y_car",
      "axis_origin": [0, 0, 0],
      "axis_direction": [0, 1, 0],
      "limits": {
        "min": 0,
        "max": 600
      }
    },
    {
      "id": "Z",
      "type": "prismatic",
      "parent_body": "y_car",
      "child_body": "z_car",
      "axis_origin": [0, 0, 0],
      "axis_direction": [0, 0, 1],
      "limits": {
        "min": 0,
        "max": 500
      }
    }
  ],
  "chain": ["X", "Y", "Z"]
})";
        std::ofstream file("/tmp/pkm_loader_tests/threejoint.pkm");
        file << pkm_content;
        file.close();
        
        auto result = KinematicLoader::load_from_json("/tmp/pkm_loader_tests/threejoint.pkm");
        test("Load PKM with three joints", result.success &&
             result.pkm.joints.size() == 3);
        test("Chain parsed correctly", result.pkm.chain.size() == 3 &&
             result.pkm.chain[0] == "X" &&
             result.pkm.chain[1] == "Y" &&
             result.pkm.chain[2] == "Z");
        test("All joint IDs parsed", result.pkm.joints.size() == 3 &&
             result.pkm.joints[0].id == "X" &&
             result.pkm.joints[1].id == "Y" &&
             result.pkm.joints[2].id == "Z");
    }
    
    // Test 4: Invalid PKM file
    {
        auto result = KinematicLoader::load_from_json("/tmp/nonexistent.pkm");
        test("Nonexistent file returns error", !result.success);
    }
    
    // Test 5: PKM with missing version fails validation
    {
        std::string pkm_content = R"({
  "machine": "test_noversion",
  "units": "mm",
  "bodies": ["base"],
  "joints": [],
  "chain": []
})";
        std::ofstream file("/tmp/pkm_loader_tests/noversion.pkm");
        file << pkm_content;
        file.close();
        
        auto result = KinematicLoader::load_from_json("/tmp/pkm_loader_tests/noversion.pkm");
        test("PKM without version fails validation", !result.success);
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
        std::cout << "✅ All PKM loader tests passed" << std::endl;
        return 0;
    } else {
        std::cout << "❌ Some tests failed" << std::endl;
        return 1;
    }
}
