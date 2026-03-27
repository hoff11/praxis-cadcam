/**
 * test_determinism.cpp
 * Sprint 12 - Determinism Hard Gate Tests
 * 
 * HARD REQUIREMENTS:
 * - Same plan → same canonical output signature
 * - Signature includes: body_count, sorted volumes, sorted bboxes
 * - No time/randomness dependencies
 */

#include "TestHarness.hpp"
#include <iostream>
#include <cassert>
#include <fstream>

using namespace praxis::testing;

void test_repeated_run_same_signature() {
    std::cout << "[Determinism] repeated_run_same_signature\n";
    
    // Run 1
    fs::path test_dir1 = TestHarness::create_test_dir("determinism_run1");
    fs::path plan_file1 = test_dir1 / "plan.json";
    fs::path out_dir1 = test_dir1 / "output";
    
    json plan = TestHarness::make_two_box_plan();
    {
        std::ofstream f(plan_file1);
        f << plan.dump(2);
        f.close();
    }
    
    std::cout << "  → Run 1...\n";
    auto result1 = TestHarness::run_plan(plan_file1, out_dir1);
    if (!result1.success()) {
        std::cerr << "  ✗ Run 1 failed\n" << result1.stderr_output << "\n";
        assert(false);
    }
    
    fs::path state1 = out_dir1 / "state.step";
    TestHarness::assert_file_exists(state1, "run 1 state");
    auto sig1 = TestHarness::compute_signature(state1);
    
    // Run 2 (fresh directory)
    fs::path test_dir2 = TestHarness::create_test_dir("determinism_run2");
    fs::path plan_file2 = test_dir2 / "plan.json";
    fs::path out_dir2 = test_dir2 / "output";
    
    {
        std::ofstream f(plan_file2);
        f << plan.dump(2);
        f.close();
    }
    
    std::cout << "  → Run 2...\n";
    auto result2 = TestHarness::run_plan(plan_file2, out_dir2);
    if (!result2.success()) {
        std::cerr << "  ✗ Run 2 failed\n" << result2.stderr_output << "\n";
        assert(false);
    }
    
    fs::path state2 = out_dir2 / "state.step";
    TestHarness::assert_file_exists(state2, "run 2 state");
    auto sig2 = TestHarness::compute_signature(state2);
    
    // HARD GATE: Signatures must be identical
    if (!(sig1 == sig2)) {
        std::cerr << "  ✗ Signatures differ!\n";
        std::cerr << "Run 1:\n" << sig1.to_json().dump(2) << "\n";
        std::cerr << "Run 2:\n" << sig2.to_json().dump(2) << "\n";
        assert(false && "Determinism violated: same plan produced different signatures");
    }
    
    std::cout << "  ✓ Signatures identical\n";
    std::cout << "    Body count: " << sig1.body_count << "\n";
    std::cout << "    Volumes: [";
    for (size_t i = 0; i < sig1.volumes.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << sig1.volumes[i];
    }
    std::cout << "]\n";
    
    std::cout << "  ✓ SHA-256 match: " << sig1.to_sha256().substr(0, 16) << "...\n";
    
    std::cout << "  ✅ DETERMINISM VALIDATED\n";
}

void test_signature_stability() {
    std::cout << "[Determinism] signature_stability\n";
    
    // Create a plan and compute its signature
    fs::path test_dir = TestHarness::create_test_dir("signature_stability");
    fs::path plan_file = test_dir / "plan.json";
    fs::path out_dir = test_dir / "output";
    
    json plan = TestHarness::make_two_box_plan();
    {
        std::ofstream f(plan_file);
        f << plan.dump(2);
        f.close();
    }
    
    auto result = TestHarness::run_plan(plan_file, out_dir);
    if (!result.success()) {
        std::cerr << "  ✗ Plan failed\n";
        assert(false);
    }
    
    fs::path state_step = out_dir / "state.step";
    auto sig = TestHarness::compute_signature(state_step);
    
    // HARD GATE: Signature properties must be correct
    if (sig.body_count != 2) {
        std::cerr << "  ✗ Expected 2 bodies, signature has " << sig.body_count << "\n";
        assert(false);
    }
    
    if (sig.volumes.size() != 2) {
        std::cerr << "  ✗ Expected 2 volumes in signature\n";
        assert(false);
    }
    
    // Volumes must be sorted (determinism requirement)
    if (sig.volumes[0] > sig.volumes[1]) {
        std::cerr << "  ✗ Volumes not sorted in signature\n";
        assert(false);
    }
    
    // Must have bboxes
    if (sig.bbox_strings.size() != 2) {
        std::cerr << "  ✗ Expected 2 bbox strings\n";
        assert(false);
    }
    
    // Bboxes must be sorted
    if (sig.bbox_strings[0] > sig.bbox_strings[1]) {
        std::cerr << "  ✗ Bboxes not sorted in signature\n";
        assert(false);
    }
    
    std::cout << "  ✓ Signature structure valid\n";
    std::cout << "  ✓ Volumes sorted: " << sig.volumes[0] << " < " << sig.volumes[1] << "\n";
    std::cout << "  ✓ Bboxes sorted\n";
    
    std::cout << "  ✅ SIGNATURE STABILITY VALIDATED\n";
}

int main() {
    std::cout << "\n=== Sprint 12: Determinism Tests (HARD GATE) ===\n\n";
    
    try {
        test_repeated_run_same_signature();
        std::cout << "\n";
        test_signature_stability();
        
        std::cout << "\n✅ All determinism tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception\n";
        return 1;
    }
}
