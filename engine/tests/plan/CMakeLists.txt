cmake_minimum_required(VERSION 3.20)

# Add PlanHistory test executable
add_executable(test_plan_history
    test_plan_history.cpp
    ../../src/plan/PlanHistory.cpp
    ../../src/plan/ExecutionGraph.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/cache/KernelOpHasher.cpp
)

target_include_directories(test_plan_history PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link OpenSSL for hashing
target_link_libraries(test_plan_history
    OpenSSL::Crypto
    stdc++fs
)

# Register test
add_test(NAME PlanHistoryTest COMMAND test_plan_history)

# Add PlanDeterminism test executable (EPIC 1.6.3)
add_executable(test_plan_determinism
    test_plan_determinism.cpp
    ../../src/plan/PlanHistory.cpp
    ../../src/plan/ExecutionGraph.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/cache/KernelOpHasher.cpp
)

target_include_directories(test_plan_determinism PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link OpenSSL for hashing
target_link_libraries(test_plan_determinism
    OpenSSL::Crypto
    stdc++fs
)

# Register test
add_test(NAME PlanDeterminismTest COMMAND test_plan_determinism)
