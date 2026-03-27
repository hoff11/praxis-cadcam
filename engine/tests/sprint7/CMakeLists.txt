# Sprint 7 Tests - Semantic Interaction Contract (SIC)

# Common test dependencies
set(SPRINT7_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/sprint7
)

# Selector Grammar Tests (Epic 1)
# Legacy SelectorExecutor-based tests are temporarily disabled after C.4/C.5 parser/resolver split.
# TODO: Reintroduce parser/resolver-specific tests under selector_grammar.
# add_subdirectory(selector_grammar)

# Kernel Compliance Tests (Epic 3)
add_executable(test_occt_compliance test_occt_compliance.cpp ../../src/OCCTInspector.cpp ../../src/CanonicalFormat.cpp)
target_include_directories(test_occt_compliance PRIVATE ${SPRINT7_INCLUDES})
target_compile_definitions(test_occt_compliance PRIVATE REPO_ROOT_DIR="${CMAKE_SOURCE_DIR}/.." TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
target_link_libraries(test_occt_compliance
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKGeomAlgo
    TKGeomBase
    TKG2d
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
)
find_library(TKXDESTEP_LIB TKXDESTEP)
if(TKXDESTEP_LIB)
    target_link_libraries(test_occt_compliance TKXDESTEP)
endif()
add_test(NAME occt_compliance COMMAND test_occt_compliance)

# Multi-Body Determinism Tests (Epic 3 extension)
add_executable(test_occt_multibody test_occt_multibody.cpp ../../src/OCCTInspector.cpp ../../src/CanonicalFormat.cpp)
target_include_directories(test_occt_multibody PRIVATE ${SPRINT7_INCLUDES})
target_compile_definitions(test_occt_multibody PRIVATE REPO_ROOT_DIR="${CMAKE_SOURCE_DIR}/.." TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
target_link_libraries(test_occt_multibody
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKGeomAlgo
    TKGeomBase
    TKG2d
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
)
if(TKXDESTEP_LIB)
    target_link_libraries(test_occt_multibody TKXDESTEP)
endif()
add_test(NAME occt_multibody COMMAND test_occt_multibody)

# Reference Model Tests (Epic 2)
add_executable(test_reference_roundtrip reference_model/test_reference_roundtrip.cpp ../../src/OCCTInspector.cpp ../../src/Reference.cpp ../../src/CanonicalFormat.cpp)
target_include_directories(test_reference_roundtrip PRIVATE ${SPRINT7_INCLUDES})
target_compile_definitions(test_reference_roundtrip PRIVATE REPO_ROOT_DIR="${CMAKE_SOURCE_DIR}/.." TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
target_link_libraries(test_reference_roundtrip
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKGeomAlgo
    TKGeomBase
    TKG2d
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
)
if(TKXDESTEP_LIB)
    target_link_libraries(test_reference_roundtrip TKXDESTEP)
endif()
add_test(NAME reference_roundtrip COMMAND test_reference_roundtrip)

add_executable(test_json_codec reference_model/test_json_codec.cpp ../../src/OCCTInspector.cpp ../../src/Reference.cpp ../../src/CanonicalFormat.cpp)
target_include_directories(test_json_codec PRIVATE ${SPRINT7_INCLUDES})
target_compile_definitions(test_json_codec PRIVATE REPO_ROOT_DIR="${CMAKE_SOURCE_DIR}/.." TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
target_link_libraries(test_json_codec
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKGeomAlgo
    TKGeomBase
    TKG2d
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
)
if(TKXDESTEP_LIB)
    target_link_libraries(test_json_codec TKXDESTEP)
endif()
add_test(NAME json_codec COMMAND test_json_codec)

# E2E Tests (Epic 5/6) - TODO
# Negative Tests (Epic 6) - TODO
