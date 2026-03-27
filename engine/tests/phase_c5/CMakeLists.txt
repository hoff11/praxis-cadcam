# Phase C.5: Selector Resolution Tests

# Fetch Catch2
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# Test executable - include all needed source files
add_executable(test_selector_resolve
    test_selector_resolve.cpp
    ../../src/SelectorParser.cpp
    ../../src/SelectorResolver.cpp
    ../../src/Inspection.cpp
    ../../src/OCCTInspector.cpp
    ../../src/CanonicalFormat.cpp
    ../../src/kernel/ShapeHandle.cpp
    ../../src/kernel/StepIO.cpp
)

# Link dependencies
target_link_libraries(test_selector_resolve PRIVATE 
    Catch2::Catch2WithMain
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKPrim
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
)

# Include directories
target_include_directories(test_selector_resolve PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Register with CTest
add_test(NAME selector_resolve COMMAND test_selector_resolve)
