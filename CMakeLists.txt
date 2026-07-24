cmake_minimum_required(VERSION 3.20)
project(endstone_worldgen VERSION 0.4.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENDSTONE_WORLDGEN_BUILD_TESTS "Build tests" ON)
option(ENDSTONE_WORLDGEN_BUILD_PLUGIN "Build Endstone wrapper" OFF)
option(ENDSTONE_WORLDGEN_BUILD_PYTHON "Build pybind11 module" OFF)
option(ENDSTONE_WORLDGEN_BUILD_NATIVE_2630 "Build exact BDS 26.30-family adapter" ON)
set(ENDSTONE_BDS_BUILD "1.26.33" CACHE STRING "Exact BDS build: 1.26.32 or 1.26.33")
string(REPLACE "." "_" ENDSTONE_BDS_BUILD_FILE "${ENDSTONE_BDS_BUILD}")

add_library(endstone_worldgen_core STATIC
    src/chunk_buffer.cpp
    src/thread_pool.cpp
    src/generation_scheduler.cpp
    src/generation_interceptor.cpp
    src/generators.cpp
    src/neighborhood_locks.cpp
    src/version_gate.cpp
)
target_include_directories(endstone_worldgen_core PUBLIC include)
target_compile_features(endstone_worldgen_core PUBLIC cxx_std_20)
configure_file(include/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/version.h @ONLY)

if(ENDSTONE_WORLDGEN_BUILD_PLUGIN)
    if(ENDSTONE_BDS_BUILD STREQUAL "1.26.32")
        set(ENDSTONE_EXACT_TAG "v0.11.5")
    elseif(ENDSTONE_BDS_BUILD STREQUAL "1.26.33")
        set(ENDSTONE_EXACT_TAG "v0.11.6")
    elseif(ENDSTONE_WORLDGEN_BUILD_NATIVE_2630)
        message(FATAL_ERROR "Native 26.30 adapter supports only BDS 1.26.32 and 1.26.33")
    else()
        set(ENDSTONE_EXACT_TAG "v0.11.6")
    endif()

    # Private Bedrock headers use Endstone's complete dependency graph.
    # Conan generates these package configs from the exact versions pinned in conanfile.py.
    find_package(base64 REQUIRED)
    find_package(Boost REQUIRED)
    find_package(concurrentqueue REQUIRED)
    find_package(cpptrace REQUIRED)
    find_package(date REQUIRED)
    find_package(EnTT REQUIRED)
    find_package(expected-lite REQUIRED)
    find_package(fmt REQUIRED)
    find_package(funchook REQUIRED)
    find_package(glm REQUIRED)
    find_package(magic_enum REQUIRED)
    find_package(Microsoft.GSL REQUIRED)
    find_package(nlohmann_json REQUIRED)
    find_package(raknet REQUIRED)
    find_package(replxx REQUIRED)
    find_package(sentry REQUIRED)
    find_package(spdlog REQUIRED)
    find_package(tomlplusplus REQUIRED)
    find_package(zstr REQUIRED)

    include(FetchContent)
    FetchContent_Declare(endstone
        GIT_REPOSITORY https://github.com/EndstoneMC/endstone.git
        GIT_TAG ${ENDSTONE_EXACT_TAG}
    )
    FetchContent_MakeAvailable(endstone)


    set(WORLDGEN_PLUGIN_SOURCES src/plugin.cpp)
    if(ENDSTONE_WORLDGEN_BUILD_NATIVE_2630)
        list(APPEND WORLDGEN_PLUGIN_SOURCES src/bds_26_30_adapter.cpp)
    endif()

    endstone_add_plugin(worldgen_api ${WORLDGEN_PLUGIN_SOURCES})
    set_target_properties(worldgen_api PROPERTIES
        PREFIX ""
        OUTPUT_NAME "endstone_worldgen_bds_${ENDSTONE_BDS_BUILD_FILE}"
    )
    target_include_directories(worldgen_api PRIVATE
        include
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${endstone_SOURCE_DIR}/src
    )
    target_compile_definitions(worldgen_api PRIVATE
        ENDSTONE_WORLDGEN_NATIVE_2630=$<BOOL:${ENDSTONE_WORLDGEN_BUILD_NATIVE_2630}>
        ENDSTONE_WORLDGEN_BDS_BUILD="${ENDSTONE_BDS_BUILD}"
        ENTT_SPARSE_PAGE=2048
        ENTT_NO_MIXIN=1
    )
    target_link_libraries(worldgen_api PRIVATE
        endstone_worldgen_core
        aklomp::base64
        boost::boost
        concurrentqueue::concurrentqueue
        cpptrace::cpptrace
        date::date
        EnTT::EnTT
        nonstd::expected-lite
        fmt::fmt
        funchook::funchook
        glm::glm
        magic_enum::magic_enum
        Microsoft.GSL::GSL
        nlohmann_json::nlohmann_json
        raknet::raknet
        replxx::replxx
        sentry::sentry
        spdlog::spdlog
        tomlplusplus::tomlplusplus
        zstr::zstr
    )
    if(UNIX)
        target_compile_options(worldgen_api PRIVATE -fms-extensions)
    endif()

    install(TARGETS worldgen_api
        DESTINATION plugins
        RUNTIME DESTINATION plugins
        LIBRARY DESTINATION plugins
        ARCHIVE DESTINATION lib
        COMPONENT worldgen_package
    )
    install(DIRECTORY python/endstone_worldgen DESTINATION python COMPONENT worldgen_package)
    install(DIRECTORY include/endstone_worldgen DESTINATION include COMPONENT worldgen_package)
    install(FILES
        README.md
        LICENSE
        BUILD_STATUS.md
        VALIDATION_RESULTS.md
        docs/INSTALL.md
        docs/CI_TROUBLESHOOTING.md
        docs/BDS_26_30_ADAPTER.md
        docs/VANILLA_ACCELERATION.md
        DESTINATION .
        COMPONENT worldgen_package
    )
    install(FILES compatibility/versions.json DESTINATION compatibility COMPONENT worldgen_package)
endif()

if(ENDSTONE_WORLDGEN_BUILD_PYTHON)
    include(FetchContent)
    FetchContent_Declare(pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.13.6
    )
    FetchContent_MakeAvailable(pybind11)
    pybind11_add_module(_endstone_worldgen src/python_bindings.cpp)
    target_link_libraries(_endstone_worldgen PRIVATE endstone_worldgen_core)
    target_include_directories(_endstone_worldgen PRIVATE include)
endif()

if(ENDSTONE_WORLDGEN_BUILD_TESTS)
    enable_testing()
    add_executable(worldgen_tests tests/cpp/test_worldgen.cpp)
    target_link_libraries(worldgen_tests PRIVATE endstone_worldgen_core)
    add_test(NAME worldgen_tests COMMAND worldgen_tests)
endif()
