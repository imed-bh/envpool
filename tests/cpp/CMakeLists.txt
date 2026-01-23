# C++20 Module Tests with Catch2 v3

find_package(Catch2 3 REQUIRED)

# CartPole test
add_executable(test_cartpole_catch2 test_cartpole_catch2.cpp)
target_link_libraries(test_cartpole_catch2 PRIVATE
    envpool_module_env_classic_control_cartpole
    envpool_module_core_types
    Catch2::Catch2WithMain
)
target_compile_features(test_cartpole_catch2 PRIVATE cxx_std_23)

# Blackjack test
add_executable(test_blackjack_catch2 test_blackjack_catch2.cpp)
target_link_libraries(test_blackjack_catch2 PRIVATE
    envpool_module_env_toy_text_blackjack
    envpool_module_core_types
    Catch2::Catch2WithMain
)
target_compile_features(test_blackjack_catch2 PRIVATE cxx_std_23)

# Register tests with CTest
include(Catch)
catch_discover_tests(test_cartpole_catch2)
catch_discover_tests(test_blackjack_catch2)

# Custom target to run all module tests
add_custom_target(run_module_tests
    COMMAND test_cartpole_catch2
    COMMAND test_blackjack_catch2
    DEPENDS test_cartpole_catch2 test_blackjack_catch2
    COMMENT "Running C++20 module tests with Catch2..."
)
