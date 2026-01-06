# EnvPool Core Module
# Lock-free async environment pool implementation

# Core library (header-only for now, may add compiled components later)
add_library(envpool_core INTERFACE)

target_include_directories(envpool_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(envpool_core INTERFACE
    envpool_common
)

# Choose implementation based on option
if(ENVPOOL_USE_MODERN_IMPL)
    target_compile_definitions(envpool_core INTERFACE ENVPOOL_USE_MODERN_IMPL=1)
    message(STATUS "Using modern C++26 implementations")
else()
    message(STATUS "Using original implementations")
endif()

# Tests
if(ENVPOOL_BUILD_TESTS)
    # Original implementation tests
    add_executable(action_buffer_queue_test action_buffer_queue_test.cc)
    target_link_libraries(action_buffer_queue_test
        PRIVATE
            envpool_core
            GTest::gtest_main
    )
    gtest_discover_tests(action_buffer_queue_test)

    add_executable(state_buffer_queue_test state_buffer_queue_test.cc)
    target_link_libraries(state_buffer_queue_test
        PRIVATE
            envpool_core
            GTest::gtest_main
    )
    gtest_discover_tests(state_buffer_queue_test)

    add_executable(circular_buffer_test circular_buffer_test.cc)
    target_link_libraries(circular_buffer_test
        PRIVATE
            envpool_core
            GTest::gtest_main
    )
    gtest_discover_tests(circular_buffer_test)

    add_executable(state_buffer_test state_buffer_test.cc)
    target_link_libraries(state_buffer_test
        PRIVATE
            envpool_core
            GTest::gtest_main
    )
    gtest_discover_tests(state_buffer_test)

    add_executable(dict_test dict_test.cc)
    target_link_libraries(dict_test
        PRIVATE
            envpool_core
            GTest::gtest_main
    )
    gtest_discover_tests(dict_test)

    # Comprehensive tests (new)
    add_executable(action_buffer_queue_comprehensive_test
        action_buffer_queue_comprehensive_test.cc)
    target_link_libraries(action_buffer_queue_comprehensive_test
        PRIVATE
            envpool_core
            GTest::gtest_main
    )
    gtest_discover_tests(action_buffer_queue_comprehensive_test)

    add_executable(state_buffer_comprehensive_test
        state_buffer_comprehensive_test.cc)
    target_link_libraries(state_buffer_comprehensive_test
        PRIVATE
            envpool_core
            GTest::gtest_main
    )
    gtest_discover_tests(state_buffer_comprehensive_test)

    # Modern implementation tests (if enabled)
    if(ENVPOOL_USE_MODERN_IMPL)
        add_executable(circular_buffer_modern_test circular_buffer_modern_test.cc)
        target_link_libraries(circular_buffer_modern_test
            PRIVATE
                envpool_core
                GTest::gtest_main
        )
        gtest_discover_tests(circular_buffer_modern_test)

        add_executable(action_buffer_queue_modern_test
            action_buffer_queue_modern_test.cc)
        target_link_libraries(action_buffer_queue_modern_test
            PRIVATE
                envpool_core
                GTest::gtest_main
        )
        gtest_discover_tests(action_buffer_queue_modern_test)

        add_executable(state_buffer_modern_test state_buffer_modern_test.cc)
        target_link_libraries(state_buffer_modern_test
            PRIVATE
                envpool_core
                GTest::gtest_main
        )
        gtest_discover_tests(state_buffer_modern_test)

        add_executable(async_envpool_modern_test async_envpool_modern_test.cc)
        target_link_libraries(async_envpool_modern_test
            PRIVATE
                envpool_core
                envpool_dummy
                GTest::gtest_main
        )
        gtest_discover_tests(async_envpool_modern_test)
    endif()
endif()

# Benchmarks
if(ENVPOOL_BUILD_BENCHMARKS)
    add_executable(circular_buffer_benchmark circular_buffer_benchmark.cc)
    target_link_libraries(circular_buffer_benchmark
        PRIVATE
            envpool_core
            benchmark::benchmark
            benchmark::benchmark_main
    )

    add_executable(async_envpool_benchmark async_envpool_benchmark.cc)
    target_link_libraries(async_envpool_benchmark
        PRIVATE
            envpool_core
            envpool_dummy
            benchmark::benchmark
            benchmark::benchmark_main
    )

    # Install benchmarks
    install(TARGETS
        circular_buffer_benchmark
        async_envpool_benchmark
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/benchmarks
    )
endif()

# Install headers
install(FILES
    action_buffer_queue.h
    array.h
    async_envpool.h
    circular_buffer.h
    dict.h
    env.h
    env_spec.h
    envpool.h
    py_envpool.h
    spec.h
    state_buffer.h
    state_buffer_queue.h
    tuple_utils.h
    type_utils.h
    xla.h
    xla_template.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/envpool/core
)

# Install modern implementations if enabled
if(ENVPOOL_USE_MODERN_IMPL)
    install(FILES
        circular_buffer_modern.h
        action_buffer_queue_modern.h
        state_buffer_modern.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/envpool/core/modern
    )
endif()
