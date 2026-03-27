/**
 * test_e2e_plan_accumulation.cpp
 * Sprint 12 - E2E Hard Gate Test
 * 
 * HARD REQUIREMENTS:
 * - No skips, no CWD dependencies
 * - Runs real binary, inspects real artifacts
 * - Asserts accumulated state contains exactly 2 bodies
 * - Validates volumes within tolerance
 */

#include "TestHarness.hpp"
#include <iostream>
#include <cassert>
#include <fstream>

using namespace praxis::testing;

void test_two_boxes_accumulate_e2e() {
    std::cout << "[E2E] two_boxes_accumulate\n";
    
    // HERMETIC: Create fresh test directory
    fs::path test_dir = TestHarness::create_test_dir("plan_accumulation");
    fs::path plan_file = test_dir / "plan.json";
    fs::path out_dir = test_dir / "output";
    
    // Generate plan JSON
    json plan = TestHarness::make_two_box_plan();
    {
        std::ofstream f(plan_file);
        if (!f) {
            std::cerr << "  ✗ Failed to write plan file: " << plan_file << "\n";
            assert(false);
        }
        f << plan.dump(2);
        f.close();
    }
    
    std::cout << "  → Plan file: " << plan_file << "\n";
    std::cout << "  → Output dir: " << out_dir << "\n";
    std::cout << "  → Running: praxis-cad plan\n";
    
    // RUN REAL BINARY (no mocks, no stubs)
    auto result = TestHarness::run_plan(plan_file, out_dir);
    
    // HARD GATE: Must succeed
    if (!result.success()) {
        std::cerr << "  ✗ Plan execution failed (exit " << result.exit_code << ")\n";
        std::cerr << "STDOUT:\n" << result.stdout_output << "\n";
        std::cerr << "STDERR:\n" << result.stderr_output << "\n";
        assert(false && "Plan execution must succeed");
    }
    
    std::cout << "  ✓ Plan executed successfully\n";
    
    // HARD GATE: state.step must exist
    fs::path state_step = out_dir / "state.step";
    TestHarness::assert_file_exists(state_step, "state.step");
    std::cout << "  ✓ state.step exists\n";
    
    // HARD GATE: Load and inspect
    auto bodies = TestHarness::inspect_bodies(state_step);
    
    if (bodies.size() != 2) {
        std::cerr << "  ✗ Expected 2 bodies in state.step, got " << bodies.size() << "\n";
        assert(false && "Accumulated state must contain exactly 2 bodies");
    }
    std::cout << "  ✓ state.step contains exactly 2 bodies\n";
    
    // HARD GATE: Validate volumes (40³ = 64000, 30³ = 27000)
    std::vector<double> volumes;
    for (const auto& b : bodies) {
        volumes.push_back(b.volume);
    }
    std::sort(volumes.begin(), volumes.end());
    
    TestHarness::assert_near(volumes[0], 27000.0, 100.0, "small box volume");
    TestHarness::assert_near(volumes[1], 64000.0, 100.0, "large box volume");
    
    std::cout << "  ✓ Body volumes validated: " << volumes[0] << ", " << volumes[1] << "\n";
    
    // HARD GATE: Per-step artifacts exist
    TestHarness::assert_file_exists(out_dir / "step_0" / "box.step", "step_0 artifact");
    TestHarness::assert_file_exists(out_dir / "step_1" / "box.step", "step_1 artifact");
    std::cout << "  ✓ Per-step artifacts present\n";
    
    // Validate step_0 has exactly 1 body
    auto step0_bodies = TestHarness::inspect_bodies(out_dir / "step_0" / "box.step");
    if (step0_bodies.size() != 1) {
        std::cerr << "  ✗ step_0/box.step should have 1 body, got " << step0_bodies.size() << "\n";
        assert(false && "Per-step artifacts must be individual");
    }
    std::cout << "  ✓ step_0/box.step contains 1 body (not accumulated)\n";
    
    std::cout << "  ✅ ACCUMULATED STATE MODEL VALIDATED\n";
}

int main() {
    std::cout << "\n=== Sprint 12: E2E Plan Accumulation (HARD GATE) ===\n\n";
    
    try {
        test_two_boxes_accumulate_e2e();
        std::cout << "\n✅ All E2E tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception\n";
        return 1;
    }
}
