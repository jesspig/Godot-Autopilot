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
include(FetchDependencies)
include(Lto)
include(skill_gen)

# ====================================================================
# godot-autopilot — SHARED (GDExtension)
# ====================================================================
configure_file(src/core/version.hpp.in "${CMAKE_BINARY_DIR}/generated/version.hpp" @ONLY)

add_library(godot-autopilot SHARED
    src/main.cpp
    src/core/mode_detector.cpp
    src/core/log_system.cpp
    src/core/resource_registry.cpp
    src/core/scene_dirty_tracker.cpp
    src/core/server_context.cpp
    src/core/plugin_config.cpp
    src/core/export_guard.cpp
    src/core/editor_readiness.cpp
    src/core/editor_coords.cpp
    src/resources/resource_handlers.cpp
    src/resources/debugger_resources.cpp
    src/prompts/prompt_handlers.cpp
    src/prompts/prompt_create_3d_scene.cpp
    src/prompts/prompt_setup_character.cpp
    src/prompts/prompt_debug_physics.cpp
    src/prompts/prompt_setup_input_map.cpp
    src/prompts/prompt_setup_gui.cpp
    src/prompts/debugger_prompts.cpp
    src/prompts/prompt_tool_usage.cpp
    src/prompts/prompt_keycode_reference.cpp
    src/runtime/game_bridge.cpp
    src/runtime/game_bridge_input.cpp
    src/runtime/game_bridge_eval.cpp
    src/runtime/game_bridge_verify.cpp
    src/tools/animation_ops.cpp
    src/tools/audio_ops.cpp
    src/tools/autopilot_tools.cpp
    src/tools/capture_ops.cpp
    src/tools/code_exec_ops.cpp
    src/tools/config_ops.cpp
    src/tools/display_ops.cpp
    src/tools/input_map_ops.cpp
    src/tools/log_ops.cpp
    src/tools/os_ops.cpp
    src/tools/scene_tree_ops.cpp
    src/tools/theme_ops.cpp
    src/tools/test_ops.cpp
    src/tools/text_ops.cpp
    src/tools/tilemap_ops.cpp
    src/tools/tileset_ops.cpp
    src/tools/debug_ops.cpp
    src/tools/debugger_ops.cpp
    src/tools/debugger_access.cpp
    src/tools/doc_ops.cpp
    src/tools/display_window_ops.cpp
    src/tools/editor_ops.cpp
    src/tools/group_ops.cpp
    src/tools/input_ops.cpp
    src/tools/input_click_ops.cpp
    src/tools/editor_ui_ops.cpp
    src/tools/editor_ui_actions.cpp
    src/tools/nav_ops.cpp
    src/tools/physics_ops.cpp
    src/tools/property_ops.cpp
    src/tools/register_all.cpp
    src/tools/dispatch.cpp
    src/tools/analyze_ops.cpp
    src/tools/schema_builder.cpp
    src/tools/render_ops.cpp
    src/tools/environment_ops.cpp
    src/tools/resource_ops.cpp
    src/tools/runtime_ops.cpp
    src/tools/runtime_game_ops.cpp
    src/tools/scene_ops.cpp
    src/tools/scene_spec_ops.cpp
    src/tools/script_ops.cpp
    src/tools/script_patch_ops.cpp
    src/tools/spriteframes_ops.cpp
    src/tools/tool_catalog.cpp
    src/tools/tool_args.cpp
    src/tools/tool_pipeline.cpp
    src/tools/tool_invoke.cpp
    src/util/bm25_index.cpp
    src/util/variant_json.cpp
    src/util/client_config_gen.cpp
    src/util/skill_gen.cpp
    src/util/skill_content_generated.cpp
    src/util/error_util.cpp
    src/util/readback_util.cpp
    src/util/scene_verify.cpp
    src/ui/mcp_log_dock.cpp
    src/ui/mcp_config_dock.cpp)

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

