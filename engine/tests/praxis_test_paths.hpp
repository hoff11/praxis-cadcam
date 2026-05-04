/**
 * praxis_test_paths.hpp
 *
 * Shared path resolver for cadcam engine tests.
 *
 * Priority:
 *   1. PRAXIS_TEST_ROOT env var → resolves to $PRAXIS_TEST_ROOT/inputs/cadcam/fixtures
 *      PRAXIS_TEST_ROOT must be the absolute path to praxis-scenarios/testing/.
 *   2. TEST_FIXTURE_DIR compile-time define (CMake-injected fallback)
 *
 * Usage:
 *   #include "../praxis_test_paths.hpp"    // from tests/sprint*/
 *   std::string dir = resolveTestFixtureDir();
 */
#pragma once

#include <cstdlib>
#include <string>

#ifndef TEST_FIXTURE_DIR
#define TEST_FIXTURE_DIR ""
#endif

/**
 * Returns the absolute path to the test fixture directory.
 *
 * When PRAXIS_TEST_ROOT is set (canonical shared testing area), cadcam geometry
 * fixtures are expected at: $PRAXIS_TEST_ROOT/inputs/cadcam/fixtures
 *
 * Falls back to the CMake-injected TEST_FIXTURE_DIR compile-time path.
 */
inline std::string resolveTestFixtureDir() {
    if (const char* testRoot = std::getenv("PRAXIS_TEST_ROOT"); testRoot && *testRoot) {
        return std::string(testRoot) + "/inputs/cadcam/fixtures";
    }
    return std::string(TEST_FIXTURE_DIR);
}
