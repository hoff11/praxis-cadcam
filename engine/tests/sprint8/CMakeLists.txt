cmake_minimum_required(VERSION 3.20)

# Sprint 8 E2E Tests

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

# Sprint 8 E2E test
add_executable(test_sprint8_e2e
    test_sprint8_e2e.cpp
    ../../src/OCCTInspector.cpp
    ../../src/Inspection.cpp
    ../../src/Reference.cpp
    ../../src/InteractionEmit.cpp
    ../../src/InteractionPublic.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_sprint8_e2e PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/sprint8
)

target_compile_definitions(test_sprint8_e2e PRIVATE
    TEST_FIXTURE_DIR="${TEST_FIXTURE_DIR}"
    REPO_ROOT_DIR="${CMAKE_SOURCE_DIR}/.."
)

target_link_libraries(test_sprint8_e2e
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    stdc++fs
    OpenSSL::Crypto
)

# Add test to CTest
add_test(NAME Sprint8_E2E COMMAND test_sprint8_e2e)
