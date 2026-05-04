# Testing

Purpose: Define what must be tested before merge and before release.

Testing levels
- Smoke: verifies the basic path works.
- Regression: protects known bug fixes.
- Contract: validates boundary inputs and outputs.
- Determinism: confirms stable output for identical input.

When to run
- On every feature branch before requesting review.
- After any contract or parser change.
- Before tagging a release candidate.

Minimum checklist
- Build succeeds from a clean checkout.
- Smoke scenario passes.
- Changed behavior is covered by at least one regression test.
- Contract checks pass for affected interfaces.
- Determinism check passes on representative fixtures.

Test log template
- Date: YYYY-MM-DD
- Change: short name or PR
- Scope: modules touched
- Commands: exact commands run
- Result: pass | fail
- Notes: failures, flaky behavior, follow-ups

Example test log
- Date: 2026-04-19
- Change: workflow-doc bootstrap
- Scope: documentation only
- Commands: N/A
- Result: pass
- Notes: no runtime behavior changed
