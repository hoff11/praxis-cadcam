cmake_minimum_required(VERSION 3.20)

# Hardening test suite sources
set(HARDENING_A_SOURCES
    test_hardening_a.cpp
    ../../src/OCCTInspector.cpp
    ../../src/Inspection.cpp
    ../../src/Reference.cpp
    ../../src/InteractionEmit.cpp
    ../../src/InteractionPublic.cpp
    ../../src/CanonicalFormat.cpp
    ../../src/commands/SelectCommand.cpp
    ../../src/commands/ResolveCommand.cpp
)

# Test executable for Commit A: Crash-proof + Parse-proof
# Legacy selector/select command dependencies are disabled after C.4/C.5 migration.
# add_executable(test_hardening_a ${HARDENING_A_SOURCES})
# target_include_directories(test_hardening_a PRIVATE
#     ${CMAKE_SOURCE_DIR}/include
# )
# target_link_libraries(test_hardening_a
#     TKernel
#     TKMath
#     TKG3d
#     TKBRep
#     TKTopAlgo
#     TKPrim
#     TKSTEP
#     TKSTEPBase
#     TKSTEPAttr
#     stdc++fs
#     OpenSSL::Crypto
# )
# add_test(NAME hardening_a COMMAND test_hardening_a)

# Sprint 9: Canonical format unit tests
add_executable(test_canonical_format
    test_canonical_format.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_canonical_format PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_canonical_format
    stdc++fs
)

add_test(NAME canonical_format COMMAND test_canonical_format)

# Sprint 9 - Move 3: JSON escaping hardening test
add_executable(test_json_escaping
    test_json_escaping.cpp
    ../../src/CanonicalFormat.cpp
    ../../src/InteractionPublic.cpp
)

target_include_directories(test_json_escaping PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_json_escaping
    stdc++fs
)

add_test(NAME json_escaping COMMAND test_json_escaping)

# Sprint 9: Byte-identity tests
set(BYTE_IDENTITY_SOURCES
    test_byte_identity.cpp
    ../../src/OCCTInspector.cpp
    ../../src/Inspection.cpp
    ../../src/Reference.cpp
    ../../src/InteractionEmit.cpp
    ../../src/InteractionPublic.cpp
    ../../src/CanonicalFormat.cpp
    ../../src/commands/SelectCommand.cpp
    ../../src/commands/ResolveCommand.cpp
)

 # Legacy selector/select command dependencies are disabled after C.4/C.5 migration.
# add_executable(test_byte_identity ${BYTE_IDENTITY_SOURCES})
# target_include_directories(test_byte_identity PRIVATE
#     ${CMAKE_SOURCE_DIR}/include
# )
# target_compile_definitions(test_byte_identity PRIVATE
#     TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
#     REPO_ROOT_DIR="${CMAKE_SOURCE_DIR}/.."
# )
# target_link_libraries(test_byte_identity
#     TKernel
#     TKMath
#     TKG3d
#     TKBRep
#     TKTopAlgo
#     TKPrim
#     TKSTEP
#     TKSTEPBase
#     TKSTEPAttr
#     stdc++fs
#     OpenSSL::Crypto
# )
# add_test(NAME byte_identity COMMAND test_byte_identity)

# JSON Lint Test - Prevents raw string insertion regression
add_executable(test_json_lint test_json_lint.cpp)
target_compile_definitions(test_json_lint PRIVATE SRC_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME json_lint COMMAND test_json_lint)

# EPIC 1.6: Stable selectable ID tests
add_executable(test_selectable_ids
    test_selectable_ids.cpp
    ../../src/InteractionEmit.cpp
    ../../src/InteractionPublic.cpp
    ../../src/Inspection.cpp
    ../../src/OCCTInspector.cpp
    ../../src/CanonicalFormat.cpp
)

target_include_directories(test_selectable_ids PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_selectable_ids
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
    stdc++fs
    OpenSSL::Crypto
)

add_test(NAME selectable_ids COMMAND test_selectable_ids)

