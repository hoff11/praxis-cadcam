# Building

Purpose: One canonical source for local build and verification steps.

Prerequisites
- CMake installed and available in PATH.
- A supported C++ toolchain installed for this repo.
- Write access to repo-local build directory.

Default build flow
1. Configure
   - cmake -S . -B build
2. Compile
   - cmake --build build
3. Verify binary artifacts
   - confirm expected targets appear in build outputs

Clean build flow
1. Remove prior build artifacts if needed.
2. Re-run configure and compile.
3. Re-run relevant tests from testing.md.

Troubleshooting quick checks
- If configure fails, inspect toolchain and generator mismatch.
- If compile fails, rebuild after syncing dependencies and contract headers.
- If behavior differs by machine, capture exact command and environment in mistakes.md.

Change control rule
- Any PR that changes build requirements must update this file.

## Cross-Repo Testing (PRAXIS_TEST_ROOT)

Engine tests (`engine/tests/sprint8/test_sprint8_e2e`) resolve STEP geometry fixtures through a 2-tier lookup:

1. `$PRAXIS_TEST_ROOT/inputs/cadcam/fixtures` — canonical shared location (praxis-scenarios/testing)
2. CMake-injected `TEST_FIXTURE_DIR` — local build fallback for cadcam-only development

To run against the shared testing area, set `PRAXIS_TEST_ROOT` before invoking the test binary, or use the cross-repo orchestrator at `praxis-scenarios/testing/tools/run-cross-repo-tests.ps1`.

CADCAM STEP fixture migration to `praxis-scenarios/testing/inputs/cadcam/fixtures/` is **deferred** pending INIT-05 ownership resolution (D-037).

Build run log template
- Date: YYYY-MM-DD
- Machine: host or runner name
- Configure command:
- Build command:
- Result: pass | fail
- Notes:
