# Sprint 10 Test Notes

## Task 2.2: Deterministic Preview Tests

**Status:** Deferred to Sprint 11

**Reason:** The test requires access to OpCache internals that are not yet exposed in the public API. The preview classification schema has been implemented in the cache layer, but comprehensive byte-identity testing requires either:

1. OpCache to be moved to public headers, or
2. A higher-level test harness that exercises the full recipe execution path

**Current Implementation:**
- Preview classification is functional (type, previewable, semantic_type fields added to OutputArtifact)
- Metadata serialization includes preview fields
- RecipeExecutor populates preview classification based on file extension

**Test Strategy for Sprint 11:**
- Use full end-to-end recipe execution
- Compare artifact SHA256 across multiple runs
- Validate metadata consistency
- Test with both cold and warm cache states

**Acceptance Criteria (unchanged):**
- Same intent + same environment → identical preview artifact bytes
- No timestamp drift in preview outputs
- No ordering instability in preview generation
