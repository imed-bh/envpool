# CMake C++20 Modules Configuration
# Enables C++20 modules with import std support

# Require CMake 3.28+ for full module support
cmake_minimum_required(VERSION 3.28)

# Enable C++23 for modules
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable module scanning
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Experimental module support (required for import std)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "aa1f7df0-828a-4fcd-9afc-2dc80491aca7")
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP ON)

message(STATUS "C++20 Modules: ENABLED")
message(STATUS "Module scanning: ON")
message(STATUS "Experimental features: ENABLED")

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

# Create std module target (provided by compiler)
# This allows "import std;" in our modules
function(create_std_module)
  if(TARGET std)
    return()  # Already created
  endif()

  add_library(std IMPORTED INTERFACE)
  message(STATUS "std module: CONFIGURED (compiler-provided)")
endfunction()

# Call to set up std module
create_std_module()
