# ====================================================================
# LTO (Link-Time Optimization) — 全自动，Release 构建自动启用。
# 优先级: Clang ThinLTO > MSVC LTCG > GCC IPO
# ====================================================================

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "[gsd] LTO: skipped (non-Release build)")
    return()
endif()

set(GSD_LTO "OFF" CACHE INTERNAL "")

# ── Clang (including clang-cl on Windows): ThinLTO ──
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options($<$<CONFIG:Release>:-flto=thin>)
    add_link_options($<$<CONFIG:Release>:-flto=thin>)
    set(GSD_LTO "ON (ThinLTO)" CACHE INTERNAL "")
    message(STATUS "[gsd] LTO: thin (Clang ThinLTO, Release only)")
    return()
endif()

# ── MSVC cl.exe: /GL + /LTCG ──
if(MSVC)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(GSD_LTO "ON (LTCG)" CACHE INTERNAL "")
    message(STATUS "[gsd] LTO: LTCG (MSVC, Release only)")
    return()
endif()

# ── GCC / fallback IPO ──
include(CheckIPOSupported)
check_ipo_supported(RESULT _ipo_ok)
if(_ipo_ok)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(GSD_LTO "ON (IPO)" CACHE INTERNAL "")
    message(STATUS "[gsd] LTO: full (via IPO, Release only)")
else()
    message(STATUS "[gsd] LTO: skipped (not supported by compiler)")
endif()
