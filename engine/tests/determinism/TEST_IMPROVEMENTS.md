# Test Script Improvements - Phase C.3 Hardening

## CRITICAL: Face Indices Are Now Per-Body (Contract Fix)

**Date:** 2026-01-12  
**Status:** FIXED - Contract now matches correct mental model

### The Problem

Initial implementation used **global face indices** (faces 0-35 across all bodies), which violated the principle that "faces are children of bodies." This created brittleness:
- Adding/removing an early body would renumber faces in all subsequent bodies
- Per-body operations like `body(0).faces()` became awkward
- Edge adjacency `edge:N:faces:3,7` was ambiguous about which bodies faces belonged to

### The Fix

Implemented **per-body face indices** where each body has its own 0..N-1 face index space:

**Format Changes:**
- **FaceRef:** Still `face:bodyIndex:localFaceIndex` BUT now localFaceIndex is 0-based within each body
  - Body 0: `face:0:0..5` (6 faces)
  - Body 1: `face:1:0..5` (6 faces)
  - Body 5: `face:5:0..5` (6 faces)

- **EdgeRef:** Now `edge:N:faces:b0:f0,b1:f1` with explicit body:face pairs
  - Example: `edge:15:faces:0:3,1:2` (edge 15 touches face 3 of body 0 and face 2 of body 1)
  - Unambiguous and body-aware

**VMC Validation:**
- 6 bodies, each with faces 0-5
- Edges correctly reference body-scoped face pairs
- All cross-references validate
- Integrity test: **PASS**

---

## Changes Made

### 1. Fixed Unicode Encoding Issues ✓
**Problem:** Fancy Unicode arrows (→) were rendering as `â†'` in console output due to encoding mismatches.

**Fix:** Replaced all Unicode characters with ASCII equivalents:
- `→` became `->` 
- Affects: EdgeRef -> FaceRef, VertexRef -> EdgeRef, FaceRef -> BodyRef

**Impact:** CI logs will be readable across all environments.

---

### 2. Corrected Face Index Understanding ✓
**Initial Misunderstanding:** Test was written assuming face indices were **per-body** (body 0 has faces 0-5, body 1 has faces 0-5, etc.)

**Actual Implementation:** Face indices are **GLOBAL** (body 0 has faces 0-5, body 1 has faces 6-11, etc.)

**Format:** `face:bodyIndex:globalFaceIndex` where:
- `bodyIndex` indicates which body the face belongs to
- `globalFaceIndex` is the face's global sequential index (0..35 for VMC)

**Evidence from VMC artifact:**
```
Body 0: Faces 0-5    (6 faces)
Body 1: Faces 6-11   (6 faces)
Body 2: Faces 12-17  (6 faces)
Body 3: Faces 18-23  (6 faces)
Body 4: Faces 24-29  (6 faces)
Body 5: Faces 30-35  (6 faces)
Total: 36 faces, indices 0-35 (continuous)
```

**Test Fix:** Changed from per-body face tracking to global face index validation. Now correctly validates continuity 0..35.

---

### 3. Added Per-Body Face Continuity (Removed - Not Applicable)
**Initial Plan:** Validate faces are continuous 0..N within each body.

**Reality:** Not applicable since face indices are global, not per-body. Global continuity check (0..35) is sufficient.

---

### 4. Added Report File Existence Checks ✓
**Problem:** If report.json wasn't generated, test would fail with cryptic errors.

**Fix:** Added explicit checks in determinism test:
```powershell
if (-not (Test-Path $report1)) {
    Write-Host "X FAIL: Missing report.json in run1 output: $run1Dir"
    exit 1
}
```

**Impact:** Clearer failure messages if recipe doesn't emit reports.

---

## Architecture Insights

### Current Stable ID Design is **Correct with Per-Body Faces**

**Index Scoping:**
- **Bodies:** Global (0..5)
- **Faces:** **Per-body** (each body has 0..N-1)
- **Edges:** Global (0..71) - deduplicated by TShape
- **Vertices:** Global (0..47) - deduplicated by TShape

**Formats:**
- `body:N` - global body index
- `face:B:F` - body B, **local face index F within that body**
- `edge:E:faces:b0:f0,b1:f1` - global edge E, adjacent to face f0 of body b0 and face f1 of body b1
- `vertex:V:edges:e1,e2` - global vertex V, at junction of global edges e1, e2

**Why This Is Correct:**
1. Face indices are scoped to their owning body (natural mental model)
2. Edge adjacency explicitly encodes both body and face (unambiguous)
3. Adding/removing a body only affects face indices within that body
4. Per-body operations like `body(0).faces()` map naturally to index space

**Cross-Body Edges:** An edge shared between two bodies (e.g., edge 15 touches face 3 of body 0 and face 2 of body 1) correctly has `edge:15:faces:0:3,1:2` - explicit and unambiguous.

---

## Test Suite Status

### ✅ test_stable_id_integrity.ps1
**Validates:**
- All cross-references resolve with body-scoped face references
- Edge→face adjacency uses body:face pairs (e.g., `edge:15:faces:0:3,1:2`)
- Vertex→edge references (global edge indices)
- Face→body references
- Index continuity:
  - Bodies: 0..5 continuous
  - Faces: 0..5 continuous **per body** (each body independently)
  - Edges: 0..71 continuous (global)
  - Vertices: 0..47 continuous (global)
- No missing entities
- No dangling references

**Result:** PASS on VMC artifact
- 6 bodies (0-5)
- 36 faces total (each body has 0-5 = 6 faces)
- 72 edges (global, deduplicated)
- 48 vertices (global, deduplicated)

### ✅ test_stable_ids.ps1
**Validates:**
- Determinism across independent builds
- Identical stable_ids for identical geometry
- No floating-point sensitivity (pure iteration order)

**Result:** PASS - two independent builds produce identical stable_ids

---

## Recommendations

### For Contract Document (STABLE_ID_CONTRACT.md)
**Action Completed:**
- Face indices are now per-body (0..N-1 within each body)
- Edge adjacency format uses body:face pairs: `edge:N:faces:b0:f0,b1:f1`
- Cross-body edge adjacency works correctly and unambiguously
- Contract matches implementation and mental model

### For CI Integration
**Ready to deploy:**
```yaml
- name: Run stable_id tests
  run: |
    pwsh engine/tests/determinism/test_stable_id_integrity.ps1 -ReportPath path/to/report.json
    # Add determinism test once CI has build caching
```

**Watch files:**
- `engine/src/OCCTInspector.cpp` (topology enumeration)
- `engine/src/InteractionEmit.cpp` (stable_id formatting)
- `engine/include/Inspection.hpp` (interface contracts)

### No Implementation Changes Needed NOW
The current implementation is **correct after the fix**:
- Per-body face indices match mental model
- Body-aware edge adjacency removes all ambiguity  
- TShape deduplication handles shared edges/vertices correctly
- Pure iteration order ensures determinism
- Format is parseable, unambiguous, and contract-tight

**Phase C.3 is complete and production-ready with the corrected contract.**
