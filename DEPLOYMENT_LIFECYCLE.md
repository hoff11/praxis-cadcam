# Deployment Lifecycle

This document defines the user flow from machine authoring to repeated production translation.

## Lifecycle

```mermaid
flowchart LR
    A[Machine Definition\n(praxis-cadcam)] --> B[Machine Deployment\n(shop-specific)]
    B --> C[Policy Pack + Controller Base]
    C --> D[Frozen Deployment Compiler\n(governed compile authority)]
    D --> E[Production APT Input]
    E --> F[praxis-compiler]
    F --> G[Validated G-code + Diagnostics + Trace]
```

## Ownership

praxis-cadcam owns:
- Machine Definition
- Deployment composition inputs
- Machine deployment and deployment-compiler artifact definition/publication
- Controller compatibility declarations
- Machine-specific constraints

praxis-compiler owns:
- Controller semantics
- Dialect emission behavior
- Compile-time policy enforcement
- Deterministic output and compile artifacts

## Object Definitions

1. Machine Definition
- Engineering truth for geometry, kinematics, axis structure, and machine identity.

2. Machine Deployment
- A specific installed machine context in a shop.
- Adds site, installed options, deployment constraints, and selected controller base.

3. Deployment Compiler (Frozen)
- Frozen governed compile context built from:
  - machine deployment
  - policy pack
  - controller base
  - trusted overrides
  - compiler/emitter pins
- CADCAM is the owner-of-record for this artifact and its publication flow; any cross-pillar approval is a workflow gate only.

4. Compile Execution Record
- Per-run evidence produced by praxis-compiler for one source input execution.

## Practical Rule

Machine deployment alone is not sufficient for governed production translation.

Production translation should always reference a frozen deployment compiler object.
