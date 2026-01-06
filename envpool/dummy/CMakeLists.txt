# EnvPool Dummy Module
# Simple test environment for testing AsyncEnvPool

message(STATUS "Configuring dummy module")

# Dummy environment library
add_library(envpool_dummy INTERFACE)

target_include_directories(envpool_dummy INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(envpool_dummy INTERFACE
    envpool_core
)

# Tests
if(ENVPOOL_BUILD_TESTS)
    add_executable(dummy_envpool_test dummy_envpool_test.cc)
    target_link_libraries(dummy_envpool_test
        PRIVATE
            envpool_dummy
            envpool_core
            GTest::gtest_main
    )
    gtest_discover_tests(dummy_envpool_test)
endif()

# Install headers
install(FILES
    dummy_envpool.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/envpool/dummy
)
