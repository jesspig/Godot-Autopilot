# ====================================================================
# Compiler cache: sccache / ccache auto-detection.
# 完全自动：优先 sccache (支持 MSVC)，回退 ccache (GCC/Clang only)
# ====================================================================

find_program(GSD_SCCACHE NAMES sccache)
find_program(GSD_CCACHE NAMES ccache)

set(_cache_program "")

if(GSD_SCCACHE)
    set(_cache_program "${GSD_SCCACHE}")
elseif(NOT MSVC AND GSD_CCACHE)
    # ccache 不支持 MSVC
    set(_cache_program "${GSD_CCACHE}")
endif()

if(_cache_program)
    set(CMAKE_C_COMPILER_LAUNCHER "${_cache_program}" CACHE INTERNAL "")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${_cache_program}" CACHE INTERNAL "")
    get_filename_component(_cache_name "${_cache_program}" NAME)
    message(STATUS "[gsd] Cache: ${_cache_name} (${_cache_program})")
else()
    message(STATUS "[gsd] Cache: none found (direct compilation)")
endif()
