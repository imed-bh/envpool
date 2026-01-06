# EnvPool - Modern CMake + Conan Build System
# Requires: CMake 3.25+, Conan 2.x, C++23/26 compiler

cmake_minimum_required(VERSION 3.25)

# Project metadata
project(EnvPool
    VERSION 1.0.0
    DESCRIPTION "High-performance parallel RL environment pool"
    LANGUAGES CXX C
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add CMake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Options
option(ENVPOOL_BUILD_TESTS "Build tests" ON)
option(ENVPOOL_BUILD_BENCHMARKS "Build benchmarks" ON)
option(ENVPOOL_BUILD_PYTHON "Build Python bindings" ON)
option(ENVPOOL_BUILD_ATARI "Build Atari environments" ON)
option(ENVPOOL_BUILD_MUJOCO "Build MuJoCo environments" ON)
option(ENVPOOL_BUILD_VIZDOOM "Build ViZDoom environments" ON)
option(ENVPOOL_BUILD_PROCGEN "Build Procgen environments" ON)
option(ENVPOOL_ENABLE_CUDA "Enable CUDA support" OFF)
option(ENVPOOL_USE_MODERN_IMPL "Use modern C++26 implementations" ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Werror
        -Wno-unused-parameter
        -march=native
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0 -fsanitize=address -fsanitize=undefined)
        add_link_options(-fsanitize=address -fsanitize=undefined)
    endif()
elseif(MSVC)
    add_compile_options(/W4 /WX /permissive-)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /DNDEBUG)
    endif()
endif()

# Find Conan dependencies
find_package(pybind11 REQUIRED)
find_package(GTest REQUIRED)
find_package(glog REQUIRED)
find_package(gflags REQUIRED)
find_package(absl REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)

if(ENVPOOL_BUILD_ATARI)
    find_package(SDL2 REQUIRED)
endif()

if(ENVPOOL_BUILD_MUJOCO)
    # MuJoCo will be fetched via FetchContent
endif()

# FetchContent for header-only and non-Conan dependencies
include(FetchContent)

# ConcurrentQueue (header-only, fast lock-free queue)
FetchContent_Declare(
    concurrentqueue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG v1.0.4
)
FetchContent_MakeAvailable(concurrentqueue)

# ThreadPool (header-only, simple thread pool)
FetchContent_Declare(
    threadpool
    GIT_REPOSITORY https://github.com/progschj/ThreadPool.git
    GIT_TAG 9a42ec1
)
FetchContent_MakeAvailable(threadpool)

# Google Benchmark (if building benchmarks)
if(ENVPOOL_BUILD_BENCHMARKS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

# Arcade Learning Environment (for Atari)
if(ENVPOOL_BUILD_ATARI)
    FetchContent_Declare(
        ale
        GIT_REPOSITORY https://github.com/mgbellemare/Arcade-Learning-Environment.git
        GIT_TAG v0.8.1
    )
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ale)
endif()

# MuJoCo (pre-compiled binary)
if(ENVPOOL_BUILD_MUJOCO)
    FetchContent_Declare(
        mujoco
        URL https://github.com/deepmind/mujoco/releases/download/2.2.1/mujoco-2.2.1-linux-x86_64.tar.gz
        URL_HASH SHA256=d1cb3a720546240d894cd315b7fd358a2b96013a1f59b6d718036eca6f6edac2
    )
    FetchContent_MakeAvailable(mujoco)
endif()

# ViZDoom
if(ENVPOOL_BUILD_VIZDOOM)
    FetchContent_Declare(
        vizdoom
        GIT_REPOSITORY https://github.com/Farama-Foundation/ViZDoom.git
        GIT_TAG 1.1.13
    )
    FetchContent_MakeAvailable(vizdoom)
endif()

# Procgen
if(ENVPOOL_BUILD_PROCGEN)
    FetchContent_Declare(
        procgen
        GIT_REPOSITORY https://github.com/Trinkle23897/procgen.git
        GIT_TAG 0.10.8
    )
    FetchContent_MakeAvailable(procgen)
endif()

# Create interface library for common includes
add_library(envpool_common INTERFACE)
target_include_directories(envpool_common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${concurrentqueue_SOURCE_DIR}>
    $<BUILD_INTERFACE:${threadpool_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(envpool_common INTERFACE
    glog::glog
    gflags::gflags
    absl::base
    absl::strings
    absl::synchronization
)

target_compile_features(envpool_common INTERFACE cxx_std_23)

# Add subdirectories
add_subdirectory(envpool/core)
add_subdirectory(envpool/utils)

if(ENVPOOL_BUILD_ATARI)
    add_subdirectory(envpool/atari)
endif()

if(ENVPOOL_BUILD_MUJOCO)
    add_subdirectory(envpool/mujoco)
endif()

if(ENVPOOL_BUILD_VIZDOOM)
    add_subdirectory(envpool/vizdoom)
endif()

if(ENVPOOL_BUILD_PROCGEN)
    add_subdirectory(envpool/procgen)
endif()

add_subdirectory(envpool/classic_control)
add_subdirectory(envpool/toy_text)
add_subdirectory(envpool/box2d)
add_subdirectory(envpool/minigrid)
add_subdirectory(envpool/dummy)

# Python bindings
if(ENVPOOL_BUILD_PYTHON)
    add_subdirectory(envpool/python)
endif()

# Tests
if(ENVPOOL_BUILD_TESTS)
    enable_testing()
    # Tests are added in subdirectories
endif()

# Install
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS envpool_common
    EXPORT EnvPoolTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT EnvPoolTargets
    FILE EnvPoolTargets.cmake
    NAMESPACE EnvPool::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EnvPool
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EnvPoolConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/EnvPoolConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EnvPool
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/EnvPoolConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/EnvPoolConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/EnvPoolConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EnvPool
)

# CPack for packaging
set(CPACK_PACKAGE_NAME "EnvPool")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance parallel RL environment pool")
set(CPACK_PACKAGE_VENDOR "Garena Online Private Limited")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
include(CPack)

# Print configuration summary
message(STATUS "=== EnvPool Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build tests: ${ENVPOOL_BUILD_TESTS}")
message(STATUS "Build benchmarks: ${ENVPOOL_BUILD_BENCHMARKS}")
message(STATUS "Build Python bindings: ${ENVPOOL_BUILD_PYTHON}")
message(STATUS "Build Atari: ${ENVPOOL_BUILD_ATARI}")
message(STATUS "Build MuJoCo: ${ENVPOOL_BUILD_MUJOCO}")
message(STATUS "Build ViZDoom: ${ENVPOOL_BUILD_VIZDOOM}")
message(STATUS "Build Procgen: ${ENVPOOL_BUILD_PROCGEN}")
message(STATUS "Enable CUDA: ${ENVPOOL_ENABLE_CUDA}")
message(STATUS "Use modern C++26 implementations: ${ENVPOOL_USE_MODERN_IMPL}")
message(STATUS "=============================")
