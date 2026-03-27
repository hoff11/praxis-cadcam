cmake_minimum_required(VERSION 3.20)
project(praxis-cadcam VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(praxis-cadcam-cli
    src/main.cpp
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(praxis-cadcam-cli PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

install(TARGETS praxis-cadcam-cli DESTINATION bin)
