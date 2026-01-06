# EnvPool Utils Module
add_library(envpool_utils INTERFACE)
target_link_libraries(envpool_utils INTERFACE envpool_core)

# Install headers
file(GLOB UTILS_HEADERS "*.h")
install(FILES ${UTILS_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/envpool/utils
)
