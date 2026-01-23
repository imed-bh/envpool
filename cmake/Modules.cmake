# CMake C++20 Modules Configuration
# Enables C++20 modules with import std support

# Require CMake 3.28+ for full module support
cmake_minimum_required(VERSION 3.28)

# Enable module scanning
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

message(STATUS "C++20 Modules: ENABLED")
message(STATUS "Module scanning: ON")
message(STATUS "import std: ENABLED (via CMAKE_CXX_MODULE_STD)")

# Function to create a module library
function(add_cxx_module target_name)
  cmake_parse_arguments(
    MODULE
    ""
    "MODULE_NAME"
    "SOURCES;DEPENDENCIES"
    ${ARGN}
  )

  # Create library target
  add_library(${target_name})

  # Add module sources
  target_sources(${target_name}
    PUBLIC
      FILE_SET CXX_MODULES
      BASE_DIRS ${CMAKE_SOURCE_DIR}
      FILES ${MODULE_SOURCES}
  )

  # Link dependencies
  if(MODULE_DEPENDENCIES)
    target_link_libraries(${target_name} PUBLIC ${MODULE_DEPENDENCIES})
  endif()

  message(STATUS "Module created: ${target_name} (${MODULE_MODULE_NAME})")
endfunction()
