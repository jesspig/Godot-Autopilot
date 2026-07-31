cmake_minimum_required(VERSION 3.28...4.2)

# ── 编译器自动探测 (project 之前) ──
# 优先级: Clang/clang-cl > 系统默认
if(NOT DEFINED CACHE{CMAKE_CXX_COMPILER})
    if(WIN32)
        find_program(GSD_CLANG_CL NAMES clang-cl)
        if(GSD_CLANG_CL)
            set(CMAKE_C_COMPILER "${GSD_CLANG_CL}")
            set(CMAKE_CXX_COMPILER "${GSD_CLANG_CL}")
            set(CMAKE_C_FLAGS "--target=x86_64-pc-windows-msvc" CACHE STRING "")
            set(CMAKE_CXX_FLAGS "--target=x86_64-pc-windows-msvc" CACHE STRING "")
            message(STATUS "[gsd] Auto-detected: clang-cl (${GSD_CLANG_CL})")
        endif()
    else()
        find_program(GSD_CLANGXX NAMES clang++-19 clang++-18 clang++-17 clang++-16 clang++)
        if(GSD_CLANGXX)
            set(CMAKE_CXX_COMPILER "${GSD_CLANGXX}")
            get_filename_component(_cxx_name "${GSD_CLANGXX}" NAME)
            if(_cxx_name MATCHES "^clang\\+\\+-(.+)$")
                set(_ver "${CMAKE_MATCH_1}")
                find_program(GSD_CLANG NAMES "clang-${_ver}" "clang")
            else()
                find_program(GSD_CLANG NAMES "clang")
            endif()
            if(GSD_CLANG)
                set(CMAKE_C_COMPILER "${GSD_CLANG}")
            endif()
        endif()
    endif()
endif()

project(Godot-Self-Driving VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── 模块包含 ──
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── Ninja 探测 ──
find_program(GSD_NINJA NAMES ninja)
if(GSD_NINJA AND CMAKE_GENERATOR MATCHES "Ninja")
    message(STATUS "[gsd] Ninja: ${GSD_NINJA}")
elseif(GSD_NINJA AND NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(WARNING "[gsd] Ninja available (${GSD_NINJA}) but generator is '${CMAKE_GENERATOR}'. Use -G Ninja for faster builds.")
endif()

include(Platform)
include(BuildOptimization)
include(CompilerOptions)
include(Cache)
include(FetchDependencies)
include(Lto)

# ====================================================================
# godot-self-driving — SHARED (GDExtension)
# ====================================================================
add_library(godot-self-driving SHARED
    src/main.cpp
    src/core/mode_detector.cpp
    src/core/log_system.cpp
    src/core/server_context.cpp
    src/resources/resource_handlers.cpp
    src/resources/debugger_resources.cpp
    src/prompts/prompt_handlers.cpp
    src/prompts/debugger_prompts.cpp
    src/prompts/prompt_tool_usage.cpp
    src/prompts/prompt_keycode_reference.cpp
    src/tools/audio_ops.cpp
    src/tools/capture_ops.cpp
    src/tools/code_exec_ops.cpp
    src/tools/config_ops.cpp
    src/tools/display_ops.cpp
    src/tools/input_map_ops.cpp
    src/tools/os_ops.cpp
    src/tools/scene_tree_ops.cpp
    src/tools/text_ops.cpp
    src/tools/tilemap_ops.cpp
    src/tools/debug_ops.cpp
    src/tools/debugger_ops.cpp
    src/tools/doc_ops.cpp
    src/tools/editor_ops.cpp
    src/tools/group_ops.cpp
    src/tools/input_ops.cpp
    src/tools/nav_ops.cpp
    src/tools/physics_ops.cpp
    src/tools/property_ops.cpp
    src/tools/register_all.cpp
    src/tools/schema_builder.cpp
    src/tools/render_ops.cpp
    src/tools/resource_ops.cpp
    src/tools/scene_ops.cpp
    src/tools/script_ops.cpp
    src/tools/tool_catalog.cpp
    src/util/bm25_index.cpp
    src/util/variant_json.cpp
    src/ui/mcp_status_bar.cpp
    src/ui/mcp_log_dock.cpp)

target_include_directories(godot-self-driving PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(godot-self-driving PRIVATE godot-cpp mcp-server mcp-http)
if(MSVC)
    target_link_options(godot-self-driving PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:godot-cpp>")
endif()

