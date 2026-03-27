# Sprint 7 Test Suite

Comprehensive test coverage for Sprint 7: Semantic Interaction Contract + Deterministic Selection.

## Test Organization

Tests mirror the Sprint 7 acceptance criteria exactly.

### A. Selector Grammar Tests (`selector_grammar/`)

**A1. Parsing** (`test_selector_parsing.cpp`)
- Valid grammar parses successfully
- Invalid syntax → InvalidSelector
- Illegal targets per mode → ContractViolation

**A2. Determinism** (`test_selector_determinism.cpp`)
- Same artifact + selector × N runs → identical reference JSON
- Order stability for !many

**A3. Filters** (`test_filters.cpp`)
- Exact matches (=)
- Approximate matches (~= with tolerance)
- Aggregates (max, min)
- Classification required for restricted targets (edges)

**A4. Cardinality** (`test_cardinality.cpp`)
- !one with 0 → Missing
- !one with >1 → Ambiguous
- !many returns stable ordering

### B. Reference Model Tests (`reference_model/`)

**B1. Serialization** (`test_reference_serialization.cpp`)
- Encode → decode → byte-stable
- Version tags present and validated

**B2 & B3. Resolution** (`test_resolution.cpp`)
- BodyRef, FaceRef, EdgeRef resolution
- Parent resolution enforced
- Drift detection (signature mismatch → Drifted)
- Missing geometry → Missing
- Multiple matches → Ambiguous

**B4. Contract Versioning** (`test_contract_versioning.cpp`)
- Mismatched version → ContractMismatch

### C. Kernel Compliance Tests (`kernel_compliance/`)

**C1. Enumeration Stability** (`test_enumeration_stability.cpp`)
- Repeated enumeration → identical order

**C2. Inspection Consistency** (`test_inspection_consistency.cpp`)
- Computed properties stable across runs

**C3. Forbidden API Guard** (`test_forbidden_apis.cpp`)
- Sketch/history APIs inaccessible
- No mutation paths exposed

### D. End-to-End Tests (`e2e/`)

**D1. Reference Round-Trip** (`test_e2e_round_trip.cpp`)
1. Build artifact
2. Select via grammar
3. Save refs
4. Reload artifact
5. Resolve refs → success

**D2. Change Detection** (`test_e2e_change_detection.cpp`)
1. Build artifact v1
2. Save refs
3. Build artifact v2 (intent delta)
4. Resolve refs → correct failure class

**D3. Provenance Integrity** (`test_provenance.cpp`)
- Report includes selectors, refs, parameters, failures

### E. Negative / Abuse Tests (`negative/`)

**Negative Cases** (`test_negative_cases.cpp`)
- Attempt to select illegal objects (sketch, feature)
- Attempt to select by topology ID
- Attempt to auto-resolve ambiguity
- Attempt to bypass selector grammar

All must fail loudly and correctly.

## Running Tests

```bash
# Build all Sprint 7 tests
cmake --build . --target sprint7_tests

# Run all tests
ctest -R sprint7

# Run specific test group
ctest -R sprint7_selector
ctest -R sprint7_reference
ctest -R sprint7_kernel
ctest -R sprint7_e2e
ctest -R sprint7_negative
```

## Status

**Current:** All test files are STUBS. They provide structure and acceptance criteria but require implementation.

**Next Steps:**
1. Wire inspection layer to OCCT
2. Implement selector parser
3. Implement reference codec
4. Fill in test bodies with real assertions

## Test Requirements

- All tests must be **headless** (no UI, GPU, or display)
- All tests must be **repeatable** (deterministic)
- All tests are **CI-required** (must pass before merge)
- All tests must verify **exact contract compliance** (no approximations)

## Definition of Done

✅ 100% test pass rate in CI
✅ All acceptance criteria covered
✅ All contract violations cause loud failures
✅ All determinism guarantees verified

## License

MIT
