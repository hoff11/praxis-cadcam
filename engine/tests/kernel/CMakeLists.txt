cmake_minimum_required(VERSION 3.20)

# Kernel tests executable
add_executable(test_kernel_ops
    test_kernel_ops.cpp
    ../../src/kernel/ShapeHandle.cpp
    ../../src/kernel/KernelOps.cpp
    ../../src/kernel/StepIO.cpp
)

# Include directories
target_include_directories(test_kernel_ops PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link OCCT/OCE libraries
target_link_libraries(test_kernel_ops
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKPrim
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    stdc++fs
)

# C++17 standard
set_target_properties(test_kernel_ops PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add test
add_test(NAME KernelOpsTests COMMAND test_kernel_ops)

# PKM Validator tests
add_executable(test_pkm_validator
    test_pkm_validator.cpp
    ../../src/kinematics/KinematicTypes.cpp
    ../../src/kinematics/KinematicValidator.cpp
)

target_include_directories(test_pkm_validator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(test_pkm_validator PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME PKMValidatorTests COMMAND test_pkm_validator)

# Recipe Validator tests
add_executable(test_recipe_validator
    test_recipe_validator.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/recipe/RecipeValidator.cpp
)

target_include_directories(test_recipe_validator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_recipe_validator
    OpenSSL::Crypto
)

set_target_properties(test_recipe_validator PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME RecipeValidatorTests COMMAND test_recipe_validator)

# PKM Loader tests
add_executable(test_pkm_loader
    test_pkm_loader.cpp
    ../../src/kinematics/KinematicTypes.cpp
    ../../src/kinematics/KinematicValidator.cpp
    ../../src/kinematics/KinematicLoader.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_pkm_loader PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(test_pkm_loader PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME PKMLoaderTests COMMAND test_pkm_loader)

# Expression Evaluator tests
add_executable(test_expression_eval
    test_expression_eval.cpp
    ../../src/recipe/RecipeTypes.cpp
)

target_include_directories(test_expression_eval PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_expression_eval
    OpenSSL::Crypto
)

set_target_properties(test_expression_eval PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME ExpressionEvalTests COMMAND test_expression_eval)

# Recipe-PKM Validation tests
add_executable(test_recipe_pkm_validation
    test_recipe_pkm_validation.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/recipe/RecipeValidator.cpp
    ../../src/kinematics/KinematicTypes.cpp
)

target_include_directories(test_recipe_pkm_validation PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_recipe_pkm_validation
    OpenSSL::Crypto
)

set_target_properties(test_recipe_pkm_validation PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME RecipePKMValidationTests COMMAND test_recipe_pkm_validation)

# Recipe Executor tests
add_executable(test_recipe_executor
    test_recipe_executor.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/recipe/RecipeValidator.cpp
    ../../src/recipe/RecipeExecutor.cpp
    ../../src/kinematics/KinematicTypes.cpp
    ../../src/kinematics/KinematicValidator.cpp
    ../../src/kinematics/KinematicLoader.cpp
    ../../src/reasoning/ReasoningEngine.cpp
    ../../src/cache/ExecutionCache.cpp
    ../../src/cache/OpCache.cpp
    ../../src/cache/KernelOpHasher.cpp
    ../../src/plan/ExecutionGraph.cpp
    ../../src/kernel/ShapeHandle.cpp
    ../../src/CanonicalFormat.cpp
    ../../src/kernel/KernelOps.cpp
    ../../src/kernel/StepIO.cpp
    ../../src/kernel/KernelVersion.cpp
)

target_include_directories(test_recipe_executor PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_recipe_executor
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKPrim
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    stdc++fs
    OpenSSL::Crypto
)

set_target_properties(test_recipe_executor PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME RecipeExecutorTests COMMAND test_recipe_executor)

# Reasoning Engine tests
add_executable(test_reasoning_engine
    test_reasoning_engine.cpp
    ../../src/reasoning/ReasoningEngine.cpp
    ../../src/kinematics/KinematicTypes.cpp
)

target_include_directories(test_reasoning_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(test_reasoning_engine PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME ReasoningEngineTests COMMAND test_reasoning_engine)

# Recipe Composition tests
add_executable(test_recipe_composition
    test_recipe_composition.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/recipe/RecipeValidator.cpp
    ../../src/recipe/RecipeExecutor.cpp
    ../../src/reasoning/ReasoningEngine.cpp
    ../../src/kinematics/KinematicTypes.cpp
    ../../src/kinematics/KinematicValidator.cpp
    ../../src/kinematics/KinematicLoader.cpp
    ../../src/cache/ExecutionCache.cpp
    ../../src/cache/OpCache.cpp
    ../../src/CanonicalFormat.cpp
    ../../src/cache/KernelOpHasher.cpp
    ../../src/plan/ExecutionGraph.cpp
    ../../src/kernel/ShapeHandle.cpp
    ../../src/kernel/KernelOps.cpp
    ../../src/kernel/StepIO.cpp
    ../../src/kernel/KernelVersion.cpp
    ../../src/OCCTInspector.cpp
    # ../../src/Selector.cpp  # LEGACY: Removed - replaced by SelectorParser.cpp
    ../../src/Inspection.cpp
    ../../src/Reference.cpp
    ../../src/InteractionEmit.cpp
    ../../src/InteractionPublic.cpp
)

target_include_directories(test_recipe_composition PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_recipe_composition
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKPrim
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    stdc++fs
    OpenSSL::Crypto
)

set_target_properties(test_recipe_composition PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME RecipeCompositionTests COMMAND test_recipe_composition)
set_tests_properties(RecipeCompositionTests PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Plan Hash tests (Sprint 5 Phase 2)
add_executable(test_plan_hash
    test_plan_hash.cpp
    ../../src/recipe/RecipeTypes.cpp
)

target_include_directories(test_plan_hash PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_plan_hash
    OpenSSL::Crypto
)

set_target_properties(test_plan_hash PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME PlanHashTests COMMAND test_plan_hash)
# Cache behavior integration tests (Sprint 5 Phase 4)
add_executable(test_cache
    test_cache.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/recipe/RecipeValidator.cpp
    ../../src/recipe/RecipeExecutor.cpp
    ../../src/reasoning/ReasoningEngine.cpp
    ../../src/kinematics/KinematicTypes.cpp
    ../../src/kinematics/KinematicValidator.cpp
    ../../src/kinematics/KinematicLoader.cpp
    ../../src/cache/ExecutionCache.cpp
    ../../src/cache/OpCache.cpp
    ../../src/CanonicalFormat.cpp
    ../../src/cache/KernelOpHasher.cpp
    ../../src/plan/ExecutionGraph.cpp
    ../../src/kernel/ShapeHandle.cpp
    ../../src/kernel/KernelOps.cpp
    ../../src/kernel/StepIO.cpp
    ../../src/kernel/KernelVersion.cpp
)

target_include_directories(test_cache PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_cache
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKPrim
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    stdc++fs
    OpenSSL::Crypto
)

set_target_properties(test_cache PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME CacheTests COMMAND test_cache)
set_tests_properties(CacheTests PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
# Cache control flags tests (Sprint 5 Phase 6)
add_executable(test_cache_flags
    test_cache_flags.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/recipe/RecipeValidator.cpp
    ../../src/recipe/RecipeExecutor.cpp
    ../../src/reasoning/ReasoningEngine.cpp
    ../../src/kinematics/KinematicTypes.cpp
    ../../src/kinematics/KinematicValidator.cpp
    ../../src/kinematics/KinematicLoader.cpp
    ../../src/cache/ExecutionCache.cpp
    ../../src/cache/OpCache.cpp
    ../../src/CanonicalFormat.cpp
    ../../src/cache/KernelOpHasher.cpp
    ../../src/plan/ExecutionGraph.cpp
    ../../src/kernel/ShapeHandle.cpp
    ../../src/kernel/KernelOps.cpp
    ../../src/kernel/StepIO.cpp
    ../../src/kernel/KernelVersion.cpp
)

target_include_directories(test_cache_flags PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_cache_flags
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKPrim
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    stdc++fs
    OpenSSL::Crypto
)

set_target_properties(test_cache_flags PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME CacheFlagsTests COMMAND test_cache_flags)
set_tests_properties(CacheFlagsTests PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Execution Graph tests (Sprint 6 Phase 1)
add_executable(test_execution_graph
    test_execution_graph.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/plan/ExecutionGraph.cpp
    ../../src/cache/KernelOpHasher.cpp
)

target_include_directories(test_execution_graph PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_execution_graph
    OpenSSL::Crypto
)

set_target_properties(test_execution_graph PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME ExecutionGraphTests COMMAND test_execution_graph)

# Op Hash tests (Sprint 6 Phase 2)
add_executable(test_op_hash
    test_op_hash.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/plan/ExecutionGraph.cpp
    ../../src/cache/KernelOpHasher.cpp
    ../../src/kernel/KernelVersion.cpp
)

target_include_directories(test_op_hash PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_op_hash
    TKernel
    TKMath
    OpenSSL::Crypto
)

set_target_properties(test_op_hash PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME OpHashTests COMMAND test_op_hash)

# Op Cache tests (Sprint 6 Phase 3)
add_executable(test_op_cache
    test_op_cache.cpp
    ../../src/cache/OpCache.cpp
    ../../src/kernel/KernelVersion.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_op_cache PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_op_cache
    TKernel
    TKMath
    stdc++fs
    OpenSSL::Crypto
)

set_target_properties(test_op_cache PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME OpCacheTests COMMAND test_op_cache)

# Op Cache Integration tests (Sprint 6 Phase 3)
add_executable(test_op_cache_integration
    test_op_cache_integration.cpp
    ../../src/recipe/RecipeTypes.cpp
    ../../src/recipe/RecipeValidator.cpp
    ../../src/recipe/RecipeExecutor.cpp
    ../../src/reasoning/ReasoningEngine.cpp
    ../../src/kinematics/KinematicTypes.cpp
    ../../src/kinematics/KinematicValidator.cpp
    ../../src/kinematics/KinematicLoader.cpp
    ../../src/cache/ExecutionCache.cpp
    ../../src/cache/OpCache.cpp
    ../../src/CanonicalFormat.cpp
    ../../src/cache/KernelOpHasher.cpp
    ../../src/plan/ExecutionGraph.cpp
    ../../src/kernel/ShapeHandle.cpp
    ../../src/kernel/KernelOps.cpp
    ../../src/kernel/StepIO.cpp
    ../../src/kernel/KernelVersion.cpp
)

target_include_directories(test_op_cache_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_op_cache_integration
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKPrim
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    stdc++fs
    OpenSSL::Crypto
)

set_target_properties(test_op_cache_integration PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME OpCacheIntegrationTests COMMAND test_op_cache_integration)

# Concurrency torture tests (nice-to-have)
add_executable(test_cache_concurrent
    test_cache_concurrent.cpp
    ../../src/cache/OpCache.cpp
    ../../src/kernel/KernelVersion.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_cache_concurrent PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_cache_concurrent
    stdc++fs
    OpenSSL::Crypto
)

set_target_properties(test_cache_concurrent PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME CacheConcurrentTests COMMAND test_cache_concurrent)

# Orphan cleanup tests
add_executable(test_cache_orphan_cleanup
    test_cache_orphan_cleanup.cpp
    ../../src/cache/OpCache.cpp
    ../../src/kernel/KernelVersion.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_cache_orphan_cleanup PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_cache_orphan_cleanup
    stdc++fs
    OpenSSL::Crypto
)

set_target_properties(test_cache_orphan_cleanup PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME CacheOrphanCleanupTests COMMAND test_cache_orphan_cleanup)

# Cache integrity tests
add_executable(test_cache_integrity
    test_cache_integrity.cpp
    ../../src/cache/OpCache.cpp
    ../../src/kernel/KernelVersion.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_cache_integrity PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_cache_integrity
    stdc++fs
    OpenSSL::Crypto
)

set_target_properties(test_cache_integrity PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME CacheIntegrityTests COMMAND test_cache_integrity)

# Cache stats and prune tests
add_executable(test_cache_stats_prune
    test_cache_stats_prune.cpp
    ../../src/cache/OpCache.cpp
    ../../src/kernel/KernelVersion.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_cache_stats_prune PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_cache_stats_prune
    stdc++fs
    OpenSSL::Crypto
)

set_target_properties(test_cache_stats_prune PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME CacheStatsPruneTests COMMAND test_cache_stats_prune)
