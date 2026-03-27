# Praxis-Cadcam Boundary Contracts

This document defines the canonical boundary between machine-definition authoring in praxis-cadcam and compile-time consumption in praxis-compiler.

## Status

praxis-cadcam is already authoritative for machine-definition source truth (recipe + PKM + machine metadata), but boundary packaging is still being completed.

This spec establishes the minimum contract set needed to prevent drift.

## Scope

In scope:
- Canonical terms and IDs
- Ownership by contract family
- Freeze and versioning rules
- Export mapping from cadcam source truth to compiler-consumable capability profiles
- Lifecycle closure from frozen deployment compiler to compile execution record

Out of scope:
- Compiler internals and policy semantics enforcement
- UI/API transport details

## Canonical Terms

Use this vocabulary across praxis-cadcam and praxis-compiler:
- `machineId`
- `machineVersion`
- `capabilityId`
- `capabilityVersion`
- `policyPackId`
- `policyPackVersion`
- `controllerBaseId`
- `controllerBaseVersion`

Compile request alignment (compiler side):
- `machineCapabilityId`
- `targetSelection.controllerBase.id`
- `targetSelection.controllerBase.version`
- `targetSelection.machineProfile.id`
- `targetSelection.machineProfile.version`
- `targetSelection.policyPack.id`
- `targetSelection.policyPack.version`

## Contract Families

1. Machine Definition Contract
- Owner: praxis-cadcam
- Purpose: engineering truth (geometry, kinematics, machine metadata)
- Canonical schema: `contracts/boundary/schemas/machine-definition.schema.json`

2. Machine Deployment Contract
- Owner: platform boundary (authored from machine definition plus shop deployment choices)
- Purpose: shop-specific installation context for a concrete machine instance
- Canonical schema: `contracts/boundary/schemas/machine-deployment.schema.json`

3. Capability Profile Contract
- Owner: boundary spec (produced by praxis-cadcam, consumed by praxis-compiler)
- Purpose: compile-relevant limits/capabilities/constraints and stable capability identity
- Canonical schema: `contracts/boundary/schemas/capability-profile.schema.json`
- Rule: capability profile is a derived, compile-relevant projection of machine deployment, not an independent authored artifact.
- Rule: capability profile declares controller compatibility, not deployment-specific controller base identity.

4. Deployment Compiler Contract
- Owner: platform boundary (frozen governed compile context)
- Purpose: reusable authority object combining machine deployment, controller base, policy pack, trusted overrides, and compiler pins
- Canonical schema: `contracts/boundary/schemas/deployment-compiler.schema.json`

5. Policy Pack Contract
- Owner: compiler/platform boundary (praxis-compiler canonical)
- Purpose: policy identity and policy payload references
- Compiler remains semantic enforcer
- Rule: cadcam references policy-pack identity but does not author policy semantics.

6. Compile Request Contract
- Owner: orchestration boundary, consumed by praxis-compiler
- Purpose: run-level selection (`machineCapabilityId`, `targetSelection`, policies)

7. Artifact Manifest Contract
- Owner: boundary packaging layer
- Purpose: deterministic hashing, provenance, and reproducibility metadata for exported/frozen artifact sets
- Canonical schema: `contracts/boundary/schemas/manifest.schema.json`

8. Compile Execution Record Contract
- Owner: praxis-compiler
- Purpose: per-run evidence record linking request, frozen deployment compiler reference, artifacts, diagnostics, and trace
- Canonical schema: `contracts/boundary/schemas/compile-execution-record.schema.json`
- Rule: cadcam mirror schema must remain shape-compatible with compiler canonical schema; no competing record structure is allowed.

## User Flow Lifecycle

1. Machine engineer defines machine in praxis-cadcam.
2. Platform creates a shop-specific machine deployment from machine definition.
3. Shop selects policy pack and controller base.
4. System freezes a deployment compiler from deployment + policy + controller + pins.
5. Praxis-compiler repeatedly uses that frozen deployment compiler to translate production APT to governed G-code.

Lifecycle shorthand:
- Machine Definition -> Machine Deployment -> Deployment Compiler -> Compile Execution Record

Projection path shorthand:
- Machine Definition -> Machine Deployment -> Capability Profile

## Controller Ownership Split

praxis-cadcam owns:
- machine structure, geometry, kinematics, physical limits
- controller compatibility declarations
- machine-specific controller constraints

praxis-compiler owns:
- controller semantics and dialect emission behavior
- compile-time policy enforcement
- deterministic transformation and artifact emission

## Authority Rules

- Engineering truth belongs to praxis-cadcam and is exported.
- Compile truth belongs to praxis-compiler and is consumed.
- Governance truth belongs to policy/orchestration layer.
- Compiler must not consume raw PKM directly as the public compile boundary.

## Non-Negotiable Enforcement Rules

1. Compiler must not consume machine-definition or PKM directly.
2. Capability profile is the only machine-behavior input to compiler.
3. Deployment compiler must be frozen before production use.
4. Compile execution record must reference deployment compiler identity.
5. All exported or frozen artifact sets must include manifest.json.

## Freeze Rules

- All published versions are immutable.
- Re-export with changed content requires version bump.
- Exported or frozen artifact sets must include `manifest.json`.
- `manifest.json` must include deterministic content hashing, identity, and build metadata.
- Capability profile identity must be stable and reproducible from the export set.

## Export Mapping (cadcam -> compiler)

Source (cadcam):
- recipe JSON
- PKM
- machine metadata

Exported boundary artifacts:
- machine-definition.json (machine authoring truth)
- machine-deployment.json (shop-specific deployment realization)
- capability-profile.json (compiler-consumable profile)
- deployment-compiler.json (frozen governed compile context)
- geometry.step (optional but recommended for traceability)
- manifest.json (required: hashes, identity, build metadata)

Compiler input:
- capability directory containing `capability-profile.json`.
- `capability-profile.json` must expose deterministic capability identity that maps to compiler `machineCapabilityId`.
- Default mapping rule: `machineCapabilityId = capabilityId` (optionally version-qualified by policy).
- deployment-compiler must include explicit `capabilityProfileRef` so compile authority is unambiguous.

Compile output:
- compiler emits a compile execution record that references deployment compiler identity and includes artifact, diagnostics, and trace evidence.

## Near-Term Implementation Plan

1. Validate exported machine definition against:
- `contracts/boundary/schemas/machine-definition.schema.json`

2. Validate exported machine deployment against:
- `contracts/boundary/schemas/machine-deployment.schema.json`

3. Validate exported capability profile against:
- `contracts/boundary/schemas/capability-profile.schema.json`

4. Validate exported deployment compiler against:
- `contracts/boundary/schemas/deployment-compiler.schema.json`

5. Validate required manifest against:
- `contracts/boundary/schemas/manifest.schema.json`

6. Validate compile execution record against:
- `contracts/boundary/schemas/compile-execution-record.schema.json`

7. Add export command:
- `praxis-cadcam export-capability --recipe <name> --id <capability-id> --version <semver> --out <dir>`

8. Add cross-repo CI check:
- cadcam export fixture -> compiler compile fixture using exported capability directory.

## Transitional Note

Legacy TypeScript contract mirrors were moved out of active architecture into:
- `legacy_ts_reference/packages_quarantine_2026_03_27/packages/contracts/src/capabilities.ts`
- `legacy_ts_reference/packages_quarantine_2026_03_27/packages/machines/src/definitions.ts`

These are non-canonical quarantine references only. Boundary truth comes from the schemas in `contracts/boundary/schemas` and C++ validation/export paths.
