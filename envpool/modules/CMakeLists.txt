# EnvPool C++20 Modules Build Configuration

message(STATUS "Configuring C++20 modules")

# Include module support
include(${CMAKE_SOURCE_DIR}/cmake/Modules.cmake)

#===============================================================================
# Core Modules
#===============================================================================

# envpool.core.types
add_cxx_module(envpool_module_types
  MODULE_NAME "envpool.core.types"
  SOURCES core/types.cppm
  DEPENDENCIES std
)

# envpool.core.errors
add_cxx_module(envpool_module_errors
  MODULE_NAME "envpool.core.errors"
  SOURCES core/errors.cppm
  DEPENDENCIES std envpool_module_types
)

#===============================================================================
# Async Modules
#===============================================================================

# envpool.async.buffer
add_cxx_module(envpool_module_buffer
  MODULE_NAME "envpool.async.buffer"
  SOURCES async/circular_buffer.cppm
  DEPENDENCIES std envpool_module_types envpool_module_errors
)

# envpool.async.action
add_cxx_module(envpool_module_action
  MODULE_NAME "envpool.async.action"
  SOURCES async/action_queue.cppm
  DEPENDENCIES std envpool_module_types envpool_module_errors envpool_module_buffer
)

# envpool.async.state.buffer
add_cxx_module(envpool_module_state_buffer
  MODULE_NAME "envpool.async.state.buffer"
  SOURCES async/state_buffer.cppm
  DEPENDENCIES std envpool_module_types envpool_module_errors
)

# envpool.async.state.queue
add_cxx_module(envpool_module_state_queue
  MODULE_NAME "envpool.async.state.queue"
  SOURCES async/state_queue.cppm
  DEPENDENCIES std envpool_module_types envpool_module_errors envpool_module_buffer envpool_module_state_buffer
)

# envpool.async.pool
add_cxx_module(envpool_module_async_pool
  MODULE_NAME "envpool.async.pool"
  SOURCES async/async_pool.cppm
  DEPENDENCIES std envpool_module_types envpool_module_errors envpool_module_action envpool_module_state_queue
)

#===============================================================================
# Environment Modules
#===============================================================================

# envpool.env.dummy
add_cxx_module(envpool_module_dummy
  MODULE_NAME "envpool.env.dummy"
  SOURCES env/dummy.cppm
  DEPENDENCIES std envpool_module_types
)

#===============================================================================
# Module Interface Library (for easy linking)
#===============================================================================

add_library(envpool_modules INTERFACE)
target_link_libraries(envpool_modules INTERFACE
  envpool_module_types
  envpool_module_errors
  envpool_module_buffer
  envpool_module_action
  envpool_module_state_buffer
  envpool_module_state_queue
  envpool_module_async_pool
  envpool_module_dummy
)

message(STATUS "C++20 modules configured: 9 modules")
