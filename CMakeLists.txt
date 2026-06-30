cmake_minimum_required(VERSION 3.31)

project(allocators LANGUAGES C VERSION 0.1)

set(CMAKE_BUILD_TYPE Debug)

find_package(containers 0.1 REQUIRED)

add_library(allocators
            src/arena.c
            src/str_arena.c
            src/pool.c)

target_include_directories(allocators
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(allocators PUBLIC containers::containers)

add_executable(test_arena        tests/test_arena.c)
add_executable(test_str_arena    tests/test_str_arena.c)
add_executable(test_pool         tests/test_pool.c) 

target_link_libraries(test_arena PRIVATE allocators)
target_link_libraries(test_str_arena PRIVATE allocators)
target_link_libraries(test_pool PRIVATE allocators)

# ── Install ────────────────────────────────────────────────────────

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS allocators
    EXPORT allocatorsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES include/allocators.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT allocatorsTargets
    FILE allocatorsTargets.cmake
    NAMESPACE allocators::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/allocators)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/allocatorsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/allocatorsConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/allocatorsConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/allocators)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/allocatorsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/allocatorsConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/allocators)
