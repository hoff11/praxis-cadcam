cmake_minimum_required(VERSION 3.20)

# Sprint 11 Validation Tests (no GTest required - uses assert-based testing like other tests)

# Define fixture directory
set(TEST_FIXTURE_DIR "${CMAKE_SOURCE_DIR}/tests/fixtures")

# Find OpenCASCADE or OCE
find_package(OpenCASCADE QUIET)
find_package(OCE QUIET)

if(OpenCASCADE_FOUND)
    include_directories(${OpenCASCADE_INCLUDE_DIR})
elseif(OCE_FOUND)
    include_directories(${OCE_INCLUDE_DIRS})
endif()

# Task 11.1 & 11.2: Determinism tests
add_executable(test_sprint11_determinism
    test_determinism.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/recipe/RecipeValidator.cpp
    ../../src/recipe/RecipeExecutor.cpp
    ../../src/kernel/KernelOps.cpp
    ../../src/kernel/ShapeHandle.cpp
    ../../src/kernel/StepIO.cpp
    ../../src/kernel/StlIO.cpp
    ../../src/kernel/KernelVersion.cpp
    ../../src/kinematics/KinematicTypes.cpp
    ../../src/kinematics/KinematicValidator.cpp
    ../../src/kinematics/KinematicLoader.cpp
    ../../src/reasoning/ReasoningEngine.cpp
    ../../src/cache/ExecutionCache.cpp
    ../../src/cache/KernelOpHasher.cpp
    ../../src/cache/OpCache.cpp
    ../../src/plan/ExecutionGraph.cpp
    ../../src/intents/GenerateMachine3AxisVMC.cpp
    ../../src/intents/HealAndNormalize.cpp
    ../../src/intents/BuildFromRecipe.cpp
    ../../src/InteractionEmit.cpp
    ../../src/InteractionPublic.cpp
    ../../src/Inspection.cpp
    ../../src/OCCTInspector.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_sprint11_determinism PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/sprint11
)

target_compile_definitions(test_sprint11_determinism PRIVATE
    TEST_FIXTURE_DIR="${TEST_FIXTURE_DIR}"
    PRAXIS_ENGINE_VERSION="${CMAKE_PROJECT_VERSION}"
)

target_link_libraries(test_sprint11_determinism
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
)

# Register with CTest
add_test(NAME Sprint11_Determinism COMMAND test_sprint11_determinism)

# Task 11.3 & 11.4: Report emission tests (documentation/contract validation)
add_executable(test_sprint11_report_emission
    test_report_emission.cpp
)

target_link_libraries(test_sprint11_report_emission
    stdc++fs
)

add_test(NAME Sprint11_ReportEmission COMMAND test_sprint11_report_emission)

# Task 11.5: Selection integration tests (simplified - no SelectCommand dependencies)
add_executable(test_sprint11_selection_integration
    test_selection_integration.cpp
)

target_include_directories(test_sprint11_selection_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(test_sprint11_selection_integration PRIVATE
    TEST_FIXTURE_DIR="${TEST_FIXTURE_DIR}"
)

target_link_libraries(test_sprint11_selection_integration
    stdc++fs
)

add_test(NAME Sprint11_SelectionIntegration COMMAND test_sprint11_selection_integration)

# Task 11.6: Preview discovery tests
add_executable(test_sprint11_preview_discovery
    test_preview_discovery.cpp
    ../../src/cache/OpCache.cpp
    ../../src/cache/KernelOpHasher.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_sprint11_preview_discovery PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_sprint11_preview_discovery
    stdc++fs
    OpenSSL::Crypto
)

add_test(NAME Sprint11_PreviewDiscovery COMMAND test_sprint11_preview_discovery)
