cmake_minimum_required(VERSION 3.28...4.2)

# ── 编译器自动探测 (project 之前) ──
# 优先级: Clang/clang-cl > 系统默认
if(NOT DEFINED CACHE{CMAKE_CXX_COMPILER})
    if(WIN32)
        find_program(GDA_CLANG_CL NAMES clang-cl)
        if(GDA_CLANG_CL)
            set(CMAKE_C_COMPILER "${GDA_CLANG_CL}")
            set(CMAKE_CXX_COMPILER "${GDA_CLANG_CL}")
            set(CMAKE_C_FLAGS "--target=x86_64-pc-windows-msvc" CACHE STRING "")
            set(CMAKE_CXX_FLAGS "--target=x86_64-pc-windows-msvc" CACHE STRING "")
            message(STATUS "[gda] Auto-detected: clang-cl (${GDA_CLANG_CL})")
        endif()
    else()
        find_program(GDA_CLANGXX NAMES clang++-19 clang++-18 clang++-17 clang++-16 clang++)
        if(GDA_CLANGXX)
            set(CMAKE_CXX_COMPILER "${GDA_CLANGXX}")
            get_filename_component(_cxx_name "${GDA_CLANGXX}" NAME)
            if(_cxx_name MATCHES "^clang\\+\\+-(.+)$")
                set(_ver "${CMAKE_MATCH_1}")
                find_program(GDA_CLANG NAMES "clang-${_ver}" "clang")
            else()
                find_program(GDA_CLANG NAMES "clang")
            endif()
            if(GDA_CLANG)
                set(CMAKE_C_COMPILER "${GDA_CLANG}")
            endif()
        endif()
    endif()
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" GDA_VERSION)
string(STRIP "${GDA_VERSION}" GDA_VERSION)

project(Godot-Autopilot VERSION ${GDA_VERSION} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── 模块包含 ──
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── Ninja 探测 ──
find_program(GDA_NINJA NAMES ninja)
if(GDA_NINJA AND CMAKE_GENERATOR MATCHES "Ninja")
    message(STATUS "[gda] Ninja: ${GDA_NINJA}")
elseif(GDA_NINJA AND NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(WARNING "[gda] Ninja available (${GDA_NINJA}) but generator is '${CMAKE_GENERATOR}'. Use -G Ninja for faster builds.")
endif()

include(Platform)
include(BuildOptimization)
include(CompilerOptions)
include(Cache)
include(Dependencies)
include(Lto)
include(skill_gen)

# ====================================================================
# godot-autopilot — SHARED (GDExtension)
# ====================================================================
configure_file(src/core/version.hpp.in "${CMAKE_BINARY_DIR}/generated/version.hpp" @ONLY)

include(cmake/sources/gda_core.cmake)
include(cmake/sources/api_core.cmake)
include(cmake/sources/api_contract.cmake)
include(cmake/sources/api_storage.cmake)
include(cmake/sources/api_transport.cmake)
include(cmake/sources/api_cli.cmake)
include(cmake/sources/api_instance_guard.cmake)

add_library(godot-autopilot SHARED
    ${GDA_CORE_SOURCES}
    ${GDA_API_CORE_SOURCES}
    ${GDA_API_CONTRACT_SOURCES}
    ${GDA_API_STORAGE_SOURCES}
    ${GDA_API_TRANSPORT_SOURCES}
    ${GDA_API_CLI_SOURCES}
    ${GDA_API_INSTANCE_GUARD_SOURCES})

add_dependencies(godot-autopilot gda_skill_embed_header)

set_target_properties(godot-autopilot PROPERTIES
    UNITY_BUILD ${GDA_UNITY_ENABLED}
    UNITY_BUILD_BATCH_SIZE ${GDA_UNITY_BATCH})

target_include_directories(godot-autopilot PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated)

target_link_libraries(godot-autopilot PRIVATE godot-cpp mcp-server mcp-http)
if(MSVC)
    target_link_options(godot-autopilot PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:godot-cpp>")
endif()

# ====================================================================
# 测试基础设施 (默认关闭, 显式 -DGDA_ENABLE_TESTS=ON 启用)
# ====================================================================
option(GDA_ENABLE_TESTS "Enable tests" OFF)
if(GDA_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

