cmake_minimum_required(VERSION 3.20)

# Sprint 12 Tests (v0.6.0 / Phase D.1 - HARD GATE SUITE)
# No skips, no stubs, no greps - only E2E validation

# Find OpenCASCADE
find_package(OpenCASCADE QUIET)
find_package(OCE QUIET)

if(OpenCASCADE_FOUND)
    include_directories(${OpenCASCADE_INCLUDE_DIR})
elseif(OCE_FOUND)
    include_directories(${OCE_INCLUDE_DIRS})
endif()

# Shared test harness library
add_library(sprint12_harness STATIC
    TestHarness.cpp
    ../../src/kernel/StepIO.cpp
    ../../src/kernel/ShapeHandle.cpp
    ../../src/OCCTInspector.cpp
    ../../src/Inspection.cpp
    ../../src/CanonicalFormat.cpp
    ../../src/Reference.cpp
)

target_include_directories(sprint12_harness PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sprint12_harness PUBLIC
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKPrim
    TKMesh
    TKSTL
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    stdc++fs
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

# ============================================================================
# E2E Tests - TRUE HARD GATES (must succeed)
# ============================================================================

# E2E: Plan accumulation (2 boxes → compound)
add_executable(test_e2e_plan_accumulation
    test_e2e_plan_accumulation.cpp
)
target_link_libraries(test_e2e_plan_accumulation PRIVATE sprint12_harness)

# E2E: AttachFaceToFace GATE (must succeed, geometry validation)
add_executable(test_e2e_attach_gate
    test_e2e_attach_gate.cpp
)
target_link_libraries(test_e2e_attach_gate PRIVATE sprint12_harness)

# E2E: AttachFaceToFace INFRA (harness validation, graceful failures allowed)
add_executable(test_e2e_attach_infra
    test_e2e_attach_infra.cpp
)
target_link_libraries(test_e2e_attach_infra PRIVATE sprint12_harness)

# ============================================================================
# Determinism Tests (Canonical Signature Validation)
# ============================================================================

add_executable(test_determinism
    test_determinism.cpp
)
target_link_libraries(test_determinism PRIVATE sprint12_harness)

# ============================================================================
# Negative Taxonomy Tests (Error Classification)
# ============================================================================

add_executable(test_negative_taxonomy
    test_negative_taxonomy.cpp
)
target_link_libraries(test_negative_taxonomy PRIVATE sprint12_harness)

# ============================================================================
# Legacy tests (kept for transition - DEPRECATED)
# ============================================================================

add_executable(test_sprint12_plan_accumulation
    test_plan_accumulation.cpp
    ../../src/plan/PlanParser.cpp
    ../../src/kernel/StepIO.cpp
    ../../src/kernel/ShapeHandle.cpp
    ../../src/OCCTInspector.cpp
    ../../src/Inspection.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_sprint12_plan_accumulation PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_sprint12_plan_accumulation
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKPrim
    TKMesh
    TKSTL
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    stdc++fs
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

add_executable(test_sprint12_attach_face_to_face
    test_attach_face_to_face.cpp
)

target_include_directories(test_sprint12_attach_face_to_face PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# ============================================================================
# CTest Registration - Sprint12.Gate.* namespace (v0.6.0 BLOCKERS)
# ============================================================================

add_test(NAME Sprint12.Gate.E2E.PlanAccumulation 
         COMMAND test_e2e_plan_accumulation
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_test(NAME Sprint12.Gate.E2E.AttachFaceToFace 
         COMMAND test_e2e_attach_gate
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_test(NAME Sprint12.Gate.Determinism.RepeatedRun 
         COMMAND test_determinism
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_test(NAME Sprint12.Gate.Negative.Taxonomy 
         COMMAND test_negative_taxonomy
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# ============================================================================
# CTest Registration - Sprint12.Infra.* namespace (harness validation)
# ============================================================================

add_test(NAME Sprint12.Infra.AttachFaceToFaceHarness 
         COMMAND test_e2e_attach_infra
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# ============================================================================
# CTest Registration - Legacy12.* namespace (DEPRECATED, will be removed)
# ============================================================================

add_test(NAME Legacy12.PlanAccumulation_OldPattern 
         COMMAND test_sprint12_plan_accumulation
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
         
add_test(NAME Legacy12.AttachFaceToFace_GracefulFailure 
         COMMAND test_sprint12_attach_face_to_face
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
