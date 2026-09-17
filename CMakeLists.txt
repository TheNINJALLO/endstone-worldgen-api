cmake_minimum_required(VERSION 3.20)
project(endstone_worldgen VERSION 0.4.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/SOURCE_RELEASE.json" ENDSTONE_WORLDGEN_RELEASE_JSON)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/SOURCE_RELEASE.json")
string(JSON ENDSTONE_WORLDGEN_RELEASE_VERSION GET "${ENDSTONE_WORLDGEN_RELEASE_JSON}" version)
string(REPLACE "." "\\." ENDSTONE_WORLDGEN_PROJECT_VERSION_REGEX "${PROJECT_VERSION}")
if(NOT ENDSTONE_WORLDGEN_RELEASE_VERSION MATCHES "^${ENDSTONE_WORLDGEN_PROJECT_VERSION_REGEX}($|[-+])")
    message(FATAL_ERROR
        "SOURCE_RELEASE.json version '${ENDSTONE_WORLDGEN_RELEASE_VERSION}' does not match project version '${PROJECT_VERSION}'")
endif()

option(ENDSTONE_WORLDGEN_BUILD_TESTS "Build tests" ON)
option(ENDSTONE_WORLDGEN_BUILD_PLUGIN "Build Endstone wrapper" OFF)
option(ENDSTONE_WORLDGEN_BUILD_PYTHON "Build pybind11 module" OFF)
option(ENDSTONE_WORLDGEN_BUILD_LIVE_PYTHON "Build the exact native status bridge" ON)
option(ENDSTONE_WORLDGEN_BUILD_NATIVE_2630 "Build exact BDS 26.30-family adapter" ON)
set(ENDSTONE_BDS_BUILD "1.26.51" CACHE STRING "Exact BDS build (only 1.26.51 is supported)")
set_property(CACHE ENDSTONE_BDS_BUILD PROPERTY STRINGS "1.26.51")
string(REPLACE "." "_" ENDSTONE_BDS_BUILD_FILE "${ENDSTONE_BDS_BUILD}")

add_library(endstone_worldgen_core STATIC
    src/chunk_buffer.cpp
    src/thread_pool.cpp
    src/generation_scheduler.cpp
    src/generation_interceptor.cpp
    src/live_generation.cpp
    src/generators.cpp
    src/neighborhood_locks.cpp
    src/version_gate.cpp
)
target_include_directories(endstone_worldgen_core PUBLIC include)
target_compile_features(endstone_worldgen_core PUBLIC cxx_std_20)
configure_file(include/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/version.h @ONLY)

if(ENDSTONE_WORLDGEN_BUILD_PLUGIN)
    if(WIN32)
        message(FATAL_ERROR "BDS 1.26.51 chunk-source hooks are currently qualified only for Linux x86-64")
    endif()
    if(NOT DEFINED ENDSTONE_CONAN_BDS_BUILD)
        message(FATAL_ERROR
            "ENDSTONE_CONAN_BDS_BUILD is missing; configure the exact plugin with this project's Conan toolchain")
    endif()
    if(NOT ENDSTONE_CONAN_BDS_BUILD STREQUAL ENDSTONE_BDS_BUILD)
        message(FATAL_ERROR
            "Conan dependencies target BDS ${ENDSTONE_CONAN_BDS_BUILD}, but CMake targets ${ENDSTONE_BDS_BUILD}")
    endif()

    if(ENDSTONE_BDS_BUILD STREQUAL "1.26.51")
        set(ENDSTONE_EXACT_TAG "v0.11.11")
    else()
        message(FATAL_ERROR
            "The exact WorldGen adapter supports only BDS 1.26.51 with Endstone v0.11.11")
    endif()
    string(REGEX REPLACE "^v" "" ENDSTONE_EXACT_VERSION "${ENDSTONE_EXACT_TAG}")

    # Private Bedrock headers use Endstone's complete dependency graph.
    # Conan generates these package configs from the exact versions pinned in conanfile.py.
    find_package(base64 REQUIRED)
    find_package(Boost REQUIRED)
    find_package(concurrentqueue REQUIRED)
    find_package(cpptrace REQUIRED)
    find_package(date REQUIRED)
    find_package(EnTT REQUIRED)
    find_package(expected-lite REQUIRED)
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
    if(ENDSTONE_WORLDGEN_BUILD_LIVE_PYTHON)
        set(PYBIND11_FINDPYTHON ON)
        find_package(pybind11 CONFIG REQUIRED)
    endif()

    include(FetchContent)
    FetchContent_Declare(endstone
        GIT_REPOSITORY https://github.com/EndstoneMC/endstone.git
        GIT_TAG ${ENDSTONE_EXACT_TAG}
    )
    FetchContent_MakeAvailable(endstone)

    if(EXISTS "${endstone_SOURCE_DIR}/include/endstone/identifier.h")
        file(READ "${endstone_SOURCE_DIR}/include/endstone/identifier.h" ENDSTONE_IDENTIFIER_H_CONTENT)
        string(REPLACE "-> format_context::iterator" "-> typename FormatContext::iterator" ENDSTONE_IDENTIFIER_H_CONTENT "${ENDSTONE_IDENTIFIER_H_CONTENT}")
        file(WRITE "${endstone_SOURCE_DIR}/include/endstone/identifier.h" "${ENDSTONE_IDENTIFIER_H_CONTENT}")
    endif()

    if(EXISTS "${endstone_SOURCE_DIR}/src/bedrock/server/commands/command_registry.h")
        file(READ "${endstone_SOURCE_DIR}/src/bedrock/server/commands/command_registry.h" ENDSTONE_CMD_REG_CONTENT)
        string(REPLACE "R\"(, \"children\": {})\", *it->child)" "R\"(, \"children\": {})\", 0)" ENDSTONE_CMD_REG_CONTENT "${ENDSTONE_CMD_REG_CONTENT}")
        file(WRITE "${endstone_SOURCE_DIR}/src/bedrock/server/commands/command_registry.h" "${ENDSTONE_CMD_REG_CONTENT}")
    endif()

    set(ENDSTONE_RESULT_H "${endstone_SOURCE_DIR}/src/bedrock/platform/result.h")
    if(NOT EXISTS "${ENDSTONE_RESULT_H}")
        message(FATAL_ERROR "Endstone v0.11.11 result.h was not found at ${ENDSTONE_RESULT_H}")
    endif()
    file(READ "${ENDSTONE_RESULT_H}" ENDSTONE_RESULT_H_CONTENT)
    string(REPLACE "std::format(\"Error: {}\\nCall stack:{}\", error, call_stack)" "std::string(\"Error: \") + error" ENDSTONE_RESULT_H_CONTENT "${ENDSTONE_RESULT_H_CONTENT}")
    set(ENDSTONE_RESULT_ERROR_CODE_FORMAT "std::format(\"{}\", error_info.error)")
    set(ENDSTONE_RESULT_ERROR_CODE_MESSAGE "error_info.error.message()")
    string(FIND "${ENDSTONE_RESULT_H_CONTENT}" "${ENDSTONE_RESULT_ERROR_CODE_FORMAT}" ENDSTONE_RESULT_ERROR_CODE_FORMAT_INDEX)
    string(FIND "${ENDSTONE_RESULT_H_CONTENT}" "${ENDSTONE_RESULT_ERROR_CODE_MESSAGE}" ENDSTONE_RESULT_ERROR_CODE_MESSAGE_INDEX)
    if(ENDSTONE_RESULT_ERROR_CODE_FORMAT_INDEX EQUAL -1 AND ENDSTONE_RESULT_ERROR_CODE_MESSAGE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Endstone v0.11.11 result.h no longer contains the expected std::error_code formatting expression")
    endif()
    if(NOT ENDSTONE_RESULT_ERROR_CODE_FORMAT_INDEX EQUAL -1)
        string(REPLACE
            "${ENDSTONE_RESULT_ERROR_CODE_FORMAT}"
            "${ENDSTONE_RESULT_ERROR_CODE_MESSAGE}"
            ENDSTONE_RESULT_H_CONTENT
            "${ENDSTONE_RESULT_H_CONTENT}"
        )
    endif()
    file(WRITE "${ENDSTONE_RESULT_H}" "${ENDSTONE_RESULT_H_CONTENT}")

    # The adapter includes and passes exact Bedrock-owned types. Link the real
    # implementation from the matching Endstone tag; untyped no-op symbol
    # shims are undefined behaviour for non-void methods and destructors.
    if(NOT TARGET bedrock::bedrock)
        add_subdirectory(
            "${endstone_SOURCE_DIR}/src/bedrock"
            "${endstone_BINARY_DIR}/src/bedrock"
            EXCLUDE_FROM_ALL
        )
    endif()

    set(WORLDGEN_PLUGIN_SOURCES
        src/plugin.cpp
        src/native_runtime_bridge.cpp
        src/worldgen_service.cpp
    )
    if(ENDSTONE_WORLDGEN_BUILD_NATIVE_2630)
        list(APPEND WORLDGEN_PLUGIN_SOURCES src/bds_26_30_adapter.cpp)
    endif()

    endstone_add_plugin(worldgen_api ${WORLDGEN_PLUGIN_SOURCES})
    set_target_properties(worldgen_api PROPERTIES
        PREFIX ""
        OUTPUT_NAME "endstone_worldgen_bds_${ENDSTONE_BDS_BUILD_FILE}"
        BUILD_RPATH_USE_ORIGIN ON
        INSTALL_RPATH ""
        INSTALL_RPATH_USE_LINK_PATH FALSE
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    if(UNIX AND NOT APPLE)
        # Exceptions can cross the host/plugin boundary. Use the host's shared
        # unwinder; a hidden static libgcc copy aborts on host unwind contexts.
        get_target_property(native_link_libraries worldgen_api LINK_LIBRARIES)
        list(REMOVE_ITEM native_link_libraries -static-libgcc)
        set_property(TARGET worldgen_api PROPERTY LINK_LIBRARIES "${native_link_libraries}")
        target_link_libraries(worldgen_api PRIVATE -shared-libgcc)
    endif()

    target_include_directories(worldgen_api PRIVATE
        include
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${endstone_SOURCE_DIR}/src
        ${endstone_SOURCE_DIR}/include
        ${endstone_BINARY_DIR}/include
        ${endstone_BINARY_DIR}/include/generated
    )
    target_compile_definitions(worldgen_api PRIVATE
        ENDSTONE_WORLDGEN_NATIVE_2630=$<BOOL:${ENDSTONE_WORLDGEN_BUILD_NATIVE_2630}>
        ENDSTONE_WORLDGEN_BDS_BUILD="${ENDSTONE_BDS_BUILD}"
        ENDSTONE_WORLDGEN_ENDSTONE_VERSION="${ENDSTONE_EXACT_VERSION}"
        ENTT_SPARSE_PAGE=2048
        ENTT_NO_MIXIN=1
    )
    target_link_libraries(worldgen_api PRIVATE
        endstone_worldgen_core
        bedrock::bedrock
        aklomp::base64
        boost::boost
        concurrentqueue::concurrentqueue
        cpptrace::cpptrace
        date::date
        EnTT::EnTT
        nonstd::expected-lite
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
        ${CMAKE_DL_LIBS}
    )
    if(UNIX)
        target_compile_options(worldgen_api PRIVATE -fms-extensions)
        # Keep exact private ABI definitions local to this plugin. Endstone's
        # public plugin ABI is supplied by the host process, so a blanket
        # --no-undefined link would incorrectly reject those imports. The
        # post-build gate below rejects unresolved Bedrock ABI symbols.
        target_link_options(worldgen_api PRIVATE "LINKER:--exclude-libs,ALL")
    endif()

    if(UNIX AND NOT APPLE)
        add_custom_command(TARGET worldgen_api POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_FILE:FILEPATH=$<TARGET_FILE:worldgen_api>"
                "-DNM_TOOL:FILEPATH=${CMAKE_NM}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_no_undefined_bedrock_symbols.cmake"
            COMMENT "Checking the plugin for unresolved Bedrock ABI symbols"
            VERBATIM
        )
    endif()

    if(ENDSTONE_WORLDGEN_BUILD_LIVE_PYTHON)
        pybind11_add_module(_endstone_worldgen_live MODULE src/live_python_bindings.cpp)
        target_link_libraries(_endstone_worldgen_live PRIVATE endstone::endstone pybind11::module)
        target_include_directories(_endstone_worldgen_live PRIVATE include)
        target_compile_features(_endstone_worldgen_live PRIVATE cxx_std_20)
        set_target_properties(_endstone_worldgen_live PROPERTIES
            BUILD_RPATH_USE_ORIGIN ON
            INSTALL_RPATH ""
            INSTALL_RPATH_USE_LINK_PATH FALSE
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
        )
        if(UNIX AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(_endstone_worldgen_live PRIVATE -stdlib=libc++)
            target_link_options(_endstone_worldgen_live PRIVATE
                -stdlib=libc++ "LINKER:--exclude-libs,ALL")
            target_link_libraries(_endstone_worldgen_live PRIVATE
                -shared-libgcc
                -static-libstdc++
                libc++.a
                libc++abi.a
            )
        endif()
        # scripts/build_exact.py builds the plugin target explicitly. Keep the
        # bridge in that target graph so installation can never package a stale
        # or missing extension module.
        add_dependencies(worldgen_api _endstone_worldgen_live)
        if(UNIX AND NOT APPLE)
            add_custom_command(TARGET _endstone_worldgen_live POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    "-DPLUGIN_FILE:FILEPATH=$<TARGET_FILE:_endstone_worldgen_live>"
                    "-DNM_TOOL:FILEPATH=${CMAKE_NM}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_no_undefined_bedrock_symbols.cmake"
                COMMENT "Checking the live Python bridge for unresolved Bedrock ABI symbols"
                VERBATIM
            )
        endif()
    endif()

    install(TARGETS worldgen_api
        RUNTIME DESTINATION plugins COMPONENT worldgen_package
        LIBRARY DESTINATION plugins COMPONENT worldgen_package
        ARCHIVE DESTINATION lib COMPONENT worldgen_package
    )
    if(ENDSTONE_WORLDGEN_BUILD_LIVE_PYTHON)
        install(TARGETS _endstone_worldgen_live
            RUNTIME DESTINATION python COMPONENT worldgen_package
            LIBRARY DESTINATION python COMPONENT worldgen_package
        )
    endif()
    install(DIRECTORY python/endstone_worldgen DESTINATION python COMPONENT worldgen_package
        PATTERN "__pycache__" EXCLUDE
        PATTERN "*.pyc" EXCLUDE
        PATTERN "*.pyo" EXCLUDE
    )
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
    if(NOT TARGET pybind11::module)
        include(FetchContent)
        FetchContent_Declare(pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v3.0.1
        )
        FetchContent_MakeAvailable(pybind11)
    endif()
    pybind11_add_module(_endstone_worldgen src/python_bindings.cpp)
    target_link_libraries(_endstone_worldgen PRIVATE endstone_worldgen_core)
    target_include_directories(_endstone_worldgen PRIVATE include)
endif()

if(ENDSTONE_WORLDGEN_BUILD_TESTS)
    enable_testing()
    add_executable(worldgen_tests tests/cpp/test_worldgen.cpp)
    target_link_libraries(worldgen_tests PRIVATE endstone_worldgen_core)
    if(MSVC)
        target_compile_options(worldgen_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(worldgen_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME worldgen_tests COMMAND worldgen_tests)
    add_test(
        NAME worldgen_symbol_gate_accepts_clean_elf
        COMMAND "${CMAKE_COMMAND}"
            "-DNM_OUTPUT_FILE:FILEPATH=${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/nm_clean.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_no_undefined_bedrock_symbols.cmake"
    )
    add_test(
        NAME worldgen_symbol_gate_rejects_undefined_bedrock
        COMMAND "${CMAKE_COMMAND}"
            "-DNM_OUTPUT_FILE:FILEPATH=${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/nm_undefined_bedrock.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_no_undefined_bedrock_symbols.cmake"
    )
    set_tests_properties(worldgen_symbol_gate_rejects_undefined_bedrock PROPERTIES WILL_FAIL TRUE)
endif()
