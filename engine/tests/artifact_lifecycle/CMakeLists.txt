# Artifact lifecycle tests

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(test_artifact_lifecycle
    test_artifact_lifecycle.cpp
    ../../src/artifacts/ArtifactSchema.cpp
    ../../src/commands/ValidateCommand.cpp
    ../../src/commands/InspectArtifactCommand.cpp
    ../../src/commands/ExportMachineDefinitionCommand.cpp
    ../../src/commands/ExportCapabilityProfileCommand.cpp
    ../../src/commands/BundleCreateCommand.cpp
    ../../src/kinematics/KinematicTypes.cpp
    ../../src/kinematics/KinematicValidator.cpp
    ../../src/kinematics/KinematicLoader.cpp
    ../../src/CanonicalFormat.cpp
)

target_link_libraries(test_artifact_lifecycle PRIVATE
    Catch2::Catch2WithMain
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

target_include_directories(test_artifact_lifecycle PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/kinematics
)

add_test(NAME artifact_lifecycle COMMAND test_artifact_lifecycle)
