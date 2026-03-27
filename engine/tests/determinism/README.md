# Stable ID Test Suite

## Overview

This directory contains automated tests for Phase C stable_id implementation. These tests validate the **foundational guarantees** required for viewer/engine integration.

## Tests

### 1. `test_stable_ids.ps1` - Determinism Test

**Purpose**: Proves that stable_ids are identical across independent builds of the same geometry.

**What it validates**:
- Bodies generate identical `body:N` stable_ids across runs
- Faces generate identical `face:B:F` stable_ids across runs  
- Edges generate identical `edge:E:faces:...` stable_ids across runs
- Vertices generate identical `vertex:V:edges:...` stable_ids across runs

**How it works**:
1. Generates the same artifact twice with `--no-cache` (full cold build)
2. Compares all stable_ids from both `report.json` files
3. Exits 0 if identical, exits 1 if any differ

**Usage**:
```powershell
# Run with defaults (VMC artifact)
.\test_stable_ids.ps1

# Custom recipe
.\test_stable_ids.ps1 -RecipePath "/path/to/recipe.json"
```

**Expected result**: `PASS: All stable_ids are deterministic`

---

### 2. `test_stable_id_integrity.ps1` - Referential Integrity Test

**Purpose**: Validates that cross-references in stable_ids point to existing entities.

**What it validates**:
- `edge:N:faces:f1,f2` → all face indices exist in selectables
- `vertex:N:edges:e1,e2,e3` → all edge indices exist in selectables
- `face:B:F` → body index B exists in selectables
- Index continuity (no gaps: bodies 0..N-1, edges 0..M-1, vertices 0..P-1)

**How it works**:
1. Parses all stable_ids from `report.json`
2. Builds index sets for each entity type
3. Validates all cross-references resolve
4. Checks for index gaps

**Usage**:
```powershell
.\test_stable_id_integrity.ps1 -ReportPath "path/to/report.json"
```

**Expected result**: `PASS: All referential integrity checks passed`

---

## Why These Tests Matter

### Determinism Test Prevents:
- Floating-point drift causing different indices
- OCCT kernel changes breaking stable_ids
- Platform-specific ordering differences
- Cache invalidation bugs

### Integrity Test Prevents:
- Vertex referencing non-existent edge indices
- Edge referencing non-existent face indices
- Index computation bugs (gaps, duplicates)
- Viewer displaying invalid cross-references

---

## Running in CI

These tests should run on every PR that touches:
- `engine/src/OCCTInspector.cpp` (topology enumeration)
- `engine/src/InteractionEmit.cpp` (stable_id formatting)
- `engine/include/Inspection.hpp` (interface changes)

### Example GitHub Actions workflow:

```yaml
- name: Build engine
  run: ninja -C engine/build praxis-cad

- name: Test stable_id determinism
  run: pwsh engine/tests/determinism/test_stable_ids.ps1

- name: Test stable_id integrity  
  run: |
    # Generate artifact
    PRAXIS_RECIPE_PATH=recipes/vmc/vmc_v0.json \
      ./engine/build/praxis-cad --intent GenerateMachine3AxisVMC \
      --out engine/test_output --no-cache
    # Validate
    pwsh engine/tests/determinism/test_stable_id_integrity.ps1 \
      -ReportPath engine/test_output/report.json
```

---

## Phase C Milestone Criteria

For stable_ids to be considered **production-ready**, these tests must:

✅ **Pass consistently** on the same machine  
✅ **Pass across different build configurations** (Debug/Release)  
🔄 **Pass across different OCCT versions** (future hardening)  
🔄 **Pass across different platforms** (Linux/macOS/Windows - future hardening)

As of January 2026: **Determinism and integrity tests pass for Option 1 (iteration-order) implementation.**

---

## Contract Authority

These tests validate compliance with: [`docs/STABLE_ID_CONTRACT.md`](../../docs/STABLE_ID_CONTRACT.md)

Any changes to stable_id format **must update both the contract and these tests**.
