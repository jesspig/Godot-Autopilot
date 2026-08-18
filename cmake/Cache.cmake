# ====================================================================
# Compiler cache: sccache / ccache auto-detection.
# 完全自动：优先 sccache (支持 MSVC)，回退 ccache (GCC/Clang only)
# ====================================================================

find_program(GDA_SCCACHE NAMES sccache)
find_program(GDA_CCACHE NAMES ccache)

set(_cache_program "")

if(GDA_SCCACHE)
    set(_cache_program "${GDA_SCCACHE}")
elseif(NOT MSVC AND GDA_CCACHE)
    # ccache 不支持 MSVC
    set(_cache_program "${GDA_CCACHE}")
endif()

if(_cache_program)
    set(CMAKE_C_COMPILER_LAUNCHER "${_cache_program}" CACHE INTERNAL "")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${_cache_program}" CACHE INTERNAL "")
    get_filename_component(_cache_name "${_cache_program}" NAME)
    message(STATUS "[gda] Cache: ${_cache_name} (${_cache_program})")
else()
    message(STATUS "[gda] Cache: none found (direct compilation)")
endif()
