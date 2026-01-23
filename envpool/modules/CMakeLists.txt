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

# Classic Control Environments
add_cxx_module(envpool_module_cartpole
  MODULE_NAME "envpool.env.classic_control.cartpole"
  SOURCES env/classic_control/cartpole.cppm
  DEPENDENCIES std envpool_module_types
)

add_cxx_module(envpool_module_pendulum
  MODULE_NAME "envpool.env.classic_control.pendulum"
  SOURCES env/classic_control/pendulum.cppm
  DEPENDENCIES std envpool_module_types
)

add_cxx_module(envpool_module_mountain_car
  MODULE_NAME "envpool.env.classic_control.mountain_car"
  SOURCES env/classic_control/mountain_car.cppm
  DEPENDENCIES std envpool_module_types
)

add_cxx_module(envpool_module_mountain_car_continuous
  MODULE_NAME "envpool.env.classic_control.mountain_car_continuous"
  SOURCES env/classic_control/mountain_car_continuous.cppm
  DEPENDENCIES std envpool_module_types
)

add_cxx_module(envpool_module_acrobot
  MODULE_NAME "envpool.env.classic_control.acrobot"
  SOURCES env/classic_control/acrobot.cppm
  DEPENDENCIES std envpool_module_types
)

# Toy Text Environments
add_cxx_module(envpool_module_blackjack
  MODULE_NAME "envpool.env.toy_text.blackjack"
  SOURCES env/toy_text/blackjack.cppm
  DEPENDENCIES std envpool_module_types
)

add_cxx_module(envpool_module_catch
  MODULE_NAME "envpool.env.toy_text.catch"
  SOURCES env/toy_text/catch.cppm
  DEPENDENCIES std envpool_module_types
)

add_cxx_module(envpool_module_cliffwalking
  MODULE_NAME "envpool.env.toy_text.cliffwalking"
  SOURCES env/toy_text/cliffwalking.cppm
  DEPENDENCIES std envpool_module_types
)

add_cxx_module(envpool_module_frozen_lake
  MODULE_NAME "envpool.env.toy_text.frozen_lake"
  SOURCES env/toy_text/frozen_lake.cppm
  DEPENDENCIES std envpool_module_types
)

add_cxx_module(envpool_module_nchain
  MODULE_NAME "envpool.env.toy_text.nchain"
  SOURCES env/toy_text/nchain.cppm
  DEPENDENCIES std envpool_module_types
)

add_cxx_module(envpool_module_taxi
  MODULE_NAME "envpool.env.toy_text.taxi"
  SOURCES env/toy_text/taxi.cppm
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
  # Classic Control
  envpool_module_cartpole
  envpool_module_pendulum
  envpool_module_mountain_car
  envpool_module_mountain_car_continuous
  envpool_module_acrobot
  # Toy Text
  envpool_module_blackjack
  envpool_module_catch
  envpool_module_cliffwalking
  envpool_module_frozen_lake
  envpool_module_nchain
  envpool_module_taxi
)

message(STATUS "C++20 modules configured: 20 modules (9 core + 11 environments)")
