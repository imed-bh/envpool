# EnvPool2 Nanobind Python Bindings

message(STATUS "Configuring nanobind Python bindings")

# Find nanobind
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)

# Try to find nanobind, if not found, fetch it
find_package(nanobind CONFIG QUIET)
if(NOT nanobind_FOUND)
  message(STATUS "nanobind not found, fetching from GitHub")
  include(FetchContent)
  FetchContent_Declare(
    nanobind
    GIT_REPOSITORY https://github.com/wjakob/nanobind.git
    GIT_TAG v2.0.0
  )
  FetchContent_MakeAvailable(nanobind)
endif()

#===============================================================================
# Classic Control Bindings
#===============================================================================

nanobind_add_module(
  envpool2_classic_control
  STABLE_ABI
  NB_STATIC
  classic_control_bindings.cpp
)

target_link_libraries(envpool2_classic_control PRIVATE
  envpool_module_cartpole
  envpool_module_pendulum
  envpool_module_mountain_car
  envpool_module_mountain_car_continuous
  envpool_module_acrobot
)

# Set output directory
set_target_properties(envpool2_classic_control PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/envpool2"
)

#===============================================================================
# Toy Text Bindings
#===============================================================================

nanobind_add_module(
  envpool2_toy_text
  STABLE_ABI
  NB_STATIC
  toy_text_bindings.cpp
)

target_link_libraries(envpool2_toy_text PRIVATE
  envpool_module_blackjack
  envpool_module_catch
  envpool_module_cliffwalking
  envpool_module_frozen_lake
  envpool_module_nchain
  envpool_module_taxi
)

# Set output directory
set_target_properties(envpool2_toy_text PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/envpool2"
)

# Install targets
install(TARGETS envpool2_classic_control envpool2_toy_text
  LIBRARY DESTINATION envpool2
)

message(STATUS "Nanobind bindings configured: 2 modules (classic_control, toy_text)")
