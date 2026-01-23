# C++ Module Tests

# Test executables that use C++20 modules
add_executable(test_cartpole test_cartpole.cpp)
target_link_libraries(test_cartpole PRIVATE
    envpool_module_env_classic_control_cartpole
    envpool_module_core_types
)

add_executable(test_blackjack test_blackjack.cpp)
target_link_libraries(test_blackjack PRIVATE
    envpool_module_env_toy_text_blackjack
    envpool_module_core_types
)

# Make tests discoverable
enable_testing()
add_test(NAME CartPoleModuleTest COMMAND test_cartpole)
add_test(NAME BlackjackModuleTest COMMAND test_blackjack)

# Custom target to run all module tests
add_custom_target(run_module_tests
    COMMAND test_cartpole
    COMMAND test_blackjack
    DEPENDS test_cartpole test_blackjack
    COMMENT "Running C++20 module tests..."
)
