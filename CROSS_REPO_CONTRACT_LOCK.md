# Cross-Repo Contract Lock

This file locks the contract boundary between praxis-cadcam and praxis-compiler.

## Canonical Ownership

1. praxis-cadcam and boundary-owned artifacts:
- machine-definition
- machine-deployment
- capability-profile
- deployment-compiler
- manifest

Owner-of-record note:
- deployment-compiler and manifest are CADCAM-owned artifacts.
- Cross-pillar review or approval may be required operationally, but it does not create dual ownership or transfer canonical authority to praxis-compiler.

2. praxis-compiler-owned artifacts:
- controller-base
- policy-pack
- compile-request
- resolved-target
- output-artifact
- diagnostics
- trace
- compile-execution-record

## Locked Mapping Rules

1. Capability projection rule:
- machine-definition -> machine-deployment -> capability-profile
- capability-profile is a projection of deployment, not independently authored truth.

2. Compiler lookup rule:
- machineCapabilityId defaults to capabilityId.
- version qualification is policy-driven.

2a. Capability identity rule:
- canonical capability identity is capabilityId + capabilityVersion.
- machineCapabilityId, id, and version are compatibility aliases only and must not redefine identity semantics.

3. Governed compile request rule:
- compile-request with authorityMode governed must include deploymentCompilerRef.

3a. Governed request normalization rule:
- if deploymentCompilerRef is present, authorityMode must be governed.
- in governed mode, compiler derives effective targetSelection from deployment-compiler unless explicit duplication is required by a compatibility adapter.

4. Deployment compiler rule:
- deployment-compiler must include explicit capabilityProfileRef.
- deployment-compiler is the reusable frozen external authority.
- CADCAM owns the artifact definition, freeze path, and publication path for deployment-compiler.

5. Resolved target rule:
- resolved-target is compiler-internal or compiler-emitted normalized resolution derived from deployment-compiler and selected assets.

6. Execution record rule:
- compile-execution-record canonical shape is compiler-owned.
- cadcam may mirror for validation, but cannot define a competing structure.

7. Policy rule:
- policy-pack semantics are compiler/platform-owned.
- cadcam references policy identity but does not author policy semantics.

8. Manifest rule:
- every exported or frozen artifact set must include manifest.json.

## Drift Prevention Rules

1. No duplicate canonical artifact names with different shapes.
2. Any compatibility aliases must document canonical field pair explicitly.
3. Any new cross-repo field must include owner and derivation source.
4. Compiler must not consume raw PKM or machine-definition directly.

## Immediate Compatibility Checklist

1. deployment-compiler.compilerRequestMapping -> compiler effective target selection (derived internally in governed mode; optionally duplicated in request only for compatibility adapters)
2. capability-profile.machineCapabilityId -> compile-request.machineCapabilityId
3. deployment-compiler.policyPackRef -> policy-pack.id and policy-pack.version
4. deployment-compiler.controllerBaseRef -> controller-base.id and controller-base.version
5. compile-execution-record.deploymentCompilerIdentityRef -> deployment-compiler identity
