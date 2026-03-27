# Selector Grammar Tests (Epic 1)

# Test 1: Selector Parsing
add_executable(test_selector_parsing test_selector_parsing.cpp
    ${CMAKE_SOURCE_DIR}/src/Selector.cpp
    ${CMAKE_SOURCE_DIR}/src/Inspection.cpp)
target_include_directories(test_selector_parsing PRIVATE ${SPRINT7_INCLUDES})
# No OCCT libs needed for parsing tests

# Test 2: Filter Execution
add_executable(test_filters_impl test_filters_impl.cpp 
    ${CMAKE_SOURCE_DIR}/src/Selector.cpp
    ${CMAKE_SOURCE_DIR}/src/Inspection.cpp)
target_include_directories(test_filters_impl PRIVATE ${SPRINT7_INCLUDES})
# No OCCT libs needed for filter tests with fake geometry

# Register tests with CTest
add_test(NAME SelectorParsing COMMAND test_selector_parsing)
add_test(NAME FilterExecution COMMAND test_filters_impl)
