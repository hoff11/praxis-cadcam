# Sprint 11 Task 11.3: Report Emission Path Trace

**Status:** Complete
**Date:** January 8, 2026

## Report Emission Path

### Full Execution Flow

```
1. main.cpp
   ↓
2. IntentRouter::route()
   ↓
3. BuildFromRecipe intent handler
   ↓
4. RecipeExecutor::execute()
   ↓ (generates result)
5. RecipeExecutor::generate_result_summary()  ← Sprint 10 Task 3.1 (populates result.summary)
   ↓
6. main.cpp (line 208)
   ↓
7. Report::writeReport(request, result, reportPath)  ← EMISSION POINT
   ↓
8. report.json (written to disk)
```

### Key File Locations

**Execution:**
- `engine/src/main.cpp` (line 208): Calls `Report::writeReport()`
- `engine/src/recipe/RecipeExecutor.cpp` (line 467): Calls `generate_result_summary()`
- `engine/src/recipe/RecipeExecutor.cpp` (line 859): Implements `generate_result_summary()`

**Report Emission:**
- `engine/src/Report.cpp` (line 14): Implements `Report::writeReport()`
- `engine/include/praxis/Report.hpp`: Declares static `writeReport()` method

**Data Structures:**
- `engine/include/praxis/Intent.hpp` (line 107): Defines `IntentResult::Summary` struct

### Emission Authority

**Single Authoritative Emission Point:**
- `Report::writeReport()` in `engine/src/Report.cpp`
- All report JSON output flows through this single method
- No other code writes report JSON files

### Current Issue (Gap Identified)

**Sprint 10 Task 3.1 is INCOMPLETE:**

1. ✅ `IntentResult::Summary` struct exists (Intent.hpp line 107)
2. ✅ `RecipeExecutor::generate_result_summary()` implemented (RecipeExecutor.cpp line 859)
3. ✅ `generate_result_summary()` is called during execution (RecipeExecutor.cpp line 467)
4. ❌ **`Report::writeReport()` does NOT emit the summary field**

### What's Missing

The `summary` field in `IntentResult` is populated but never written to the JSON output.

**Expected JSON structure:**
```json
{
  "version": "praxis-cad/0.1.0",
  "intent": "build-from-recipe",
  "success": true,
  "summary": {
    "intent": "Create a 3-axis vertical machining center with 400×300×200 travel",
    "produced": ["Body[base]", "Body[column]"],
    "body_count": 2,
    "face_count": 0,
    "edge_count": 0,
    "artifact_count": 2,
    "previewable_outputs": ["base.step", "column.step"]
  },
  "artifacts": [...],
  ...
}
```

**Current behavior:** The `summary` field is completely absent from report output.

### Fix Required

Add summary emission to `Report::writeReport()` in Report.cpp after the success/duration fields.

### Verification

After fix:
1. Execute any recipe
2. Load report.json
3. Assert `summary` key exists
4. Assert fields match Sprint 10 specification (semantic_objects.md ordering)

## Conclusion

- Emission path is **explicitly identified**
- No implicit or undocumented behavior found
- Single authoritative emission point: `Report::writeReport()`
- **Action required:** Implement summary emission in Report.cpp
