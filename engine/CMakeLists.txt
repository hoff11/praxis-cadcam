cmake_minimum_required(VERSION 3.20)
project(praxis-cad VERSION 0.6.1 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define engine version for op hashing (Sprint 6 Phase 2)
add_compile_definitions(PRAXIS_ENGINE_VERSION="${CMAKE_PROJECT_VERSION}")
# Schema root for artifact validation
add_compile_definitions(PRAXIS_REPO_ROOT="${CMAKE_SOURCE_DIR}/..")

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++17 requires filesystem library
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find OpenCASCADE (OCE on Ubuntu 22.04)
find_package(OpenCASCADE QUIET)
find_package(OCE QUIET)

# Find OpenSSL for SHA256 hashing (Sprint 5 Phase 2)
find_package(OpenSSL REQUIRED)
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

if(OpenCASCADE_FOUND)
    message(STATUS "Found OpenCASCADE ${OpenCASCADE_VERSION}")
    message(STATUS "  Include dirs: ${OpenCASCADE_INCLUDE_DIR}")
    include_directories(${OpenCASCADE_INCLUDE_DIR})
elseif(OCE_FOUND)
    message(STATUS "Found OCE ${OCE_VERSION}")
    message(STATUS "  Include dirs: ${OCE_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${OCE_LIBRARIES}")
    include_directories(${OCE_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "Neither OpenCASCADE nor OCE found. Install with: sudo apt install liboce-foundation-dev liboce-modeling-dev liboce-ocaf-dev liboce-visualization-dev")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/IntentRouter.cpp
    src/Report.cpp
    src/InteractionEmit.cpp
    src/InteractionPublic.cpp
    src/CanonicalFormat.cpp
    src/commands/InspectCommand.cpp
    # src/commands/SelectCommand.cpp  # LEGACY: Uses old SelectorExecutor - needs C.5 migration
    src/commands/ResolveCommand.cpp
    src/commands/PlanCommand.cpp
    src/commands/PatchCommand.cpp
    src/commands/ParseSelectorCommand.cpp
    src/commands/ResolveSelectorCommand.cpp
    src/kernel/KernelOps.cpp
    src/kernel/ShapeHandle.cpp
    src/kernel/StepIO.cpp
    src/kernel/StlIO.cpp
    src/kernel/KernelVersion.cpp
    src/kinematics/KinematicTypes.cpp
    src/kinematics/KinematicValidator.cpp
    src/kinematics/KinematicLoader.cpp
    src/recipe/RecipeTypes.cpp
    src/recipe/RecipeValidator.cpp
    src/recipe/RecipeExecutor.cpp
    src/reasoning/ReasoningEngine.cpp
    src/cache/ExecutionCache.cpp
    src/cache/KernelOpHasher.cpp
    src/cache/OpCache.cpp
    src/plan/ExecutionGraph.cpp
    src/plan/PlanHistory.cpp
    src/plan/PlanParser.cpp
    src/plan/PatchParser.cpp
    src/plan/MicroPlanHistory.cpp
    src/intents/GenerateMachine3AxisVMC.cpp
    src/intents/HealAndNormalize.cpp
    src/intents/BuildFromRecipe.cpp
    src/intents/CreateBox.cpp
    src/intents/AttachFaceToFace.cpp
    src/intents/ExtractBodies.cpp
    src/OCCTInspector.cpp
    # src/Selector.cpp  # LEGACY: Removed - replaced by SelectorParser.cpp (C.4) + SelectorResolver.cpp (C.5)
    src/SelectorParser.cpp
    src/SelectorResolver.cpp
    src/Inspection.cpp
    src/Reference.cpp
)

# Artifact sources (praxis-cadcam binary only)
set(ARTIFACT_SOURCES
    src/artifacts/ArtifactSchema.cpp
    src/commands/ValidateCommand.cpp
    src/commands/InspectArtifactCommand.cpp
    src/commands/ExportMachineDefinitionCommand.cpp
    src/commands/ExportCapabilityProfileCommand.cpp
    src/commands/BundleCreateCommand.cpp
)

# Executable
add_executable(praxis-cad ${SOURCES})

# praxis-cadcam binary: engine + artifact commands
add_executable(praxis-cadcam ${SOURCES} ${ARTIFACT_SOURCES})

target_compile_definitions(praxis-cad PRIVATE PRAXIS_ENABLE_ARTIFACT_COMMANDS=0)
target_compile_definitions(praxis-cadcam PRIVATE PRAXIS_ENABLE_ARTIFACT_COMMANDS=1)

# Link OCCT/OCE libraries (minimal set for STEP I/O and basic shapes)
target_link_libraries(praxis-cad
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKPrim
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    TKMesh      # Sprint 12: STL meshing
    TKSTL       # Sprint 12: STL I/O
    stdc++fs  # C++17 filesystem
    OpenSSL::Crypto  # Sprint 5 Phase 2: SHA256 hashing
    nlohmann_json::nlohmann_json  # Robust JSON parsing
)

target_link_libraries(praxis-cadcam
    TKernel
    TKMath
    TKG3d
    TKBRep
    TKTopAlgo
    TKPrim
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    TKMesh
    TKSTL
    stdc++fs
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

# Add TKXDESTEP if available (optional for STEP)
find_library(TKXDESTEP_LIB TKXDESTEP)
if(TKXDESTEP_LIB)
    target_link_libraries(praxis-cad TKXDESTEP)
    target_link_libraries(praxis-cadcam TKXDESTEP)
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(praxis-cad PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
    target_compile_options(praxis-cadcam PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Install target (optional)
install(TARGETS praxis-cad DESTINATION bin)
install(TARGETS praxis-cadcam DESTINATION bin)

# Enable testing
enable_testing()

# Add tests subdirectory
add_subdirectory(tests/kernel)
add_subdirectory(tests/plan)
add_subdirectory(tests/sprint7)
add_subdirectory(tests/sprint8)
add_subdirectory(tests/sprint11)
add_subdirectory(tests/sprint12)
add_subdirectory(tests/hardening)
add_subdirectory(tests/phase_c4)
add_subdirectory(tests/phase_c5)
add_subdirectory(tests/artifact_lifecycle)

# Print configuration summary
message(STATUS "")
message(STATUS "Praxis-CAD Configuration:")
if(OpenCASCADE_FOUND)
    message(STATUS "  Kernel: OpenCASCADE ${OpenCASCADE_VERSION}")
elseif(OCE_FOUND)
    message(STATUS "  Kernel: OCE ${OCE_VERSION}")
endif()
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
