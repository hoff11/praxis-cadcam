# Phase C.4: Selector Grammar Parsing Tests

# Fetch Catch2
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# Test executable
add_executable(test_selector_parse
    test_selector_parse.cpp
    ../../src/SelectorParser.cpp
)

# Link dependencies
target_link_libraries(test_selector_parse PRIVATE 
    Catch2::Catch2WithMain
)

# Include directories
target_include_directories(test_selector_parse PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Register with CTest
add_test(NAME selector_parse COMMAND test_selector_parse)
