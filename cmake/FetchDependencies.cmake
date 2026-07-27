# ====================================================================
# Godot-Self-Driving 依赖管理 (FetchContent)
# 仅使用公开 GitHub 仓库，确保其他用户可直接构建。
# 第一次需要网络，之后缓存到 build/<preset>/_deps/。
# 禁止删除 _deps/，否则需要重新下载。
# ====================================================================
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ====================================================================
# godot-cpp — Godot Engine C++ 绑定层
# ====================================================================
FetchContent_Declare(godot-cpp
    GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
    GIT_TAG        10.0.0-rc1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(godot-cpp)

# ====================================================================
# mcp-cpp-sdk — MCP 协议 C++ SDK
# ====================================================================
FetchContent_Declare(mcp-cpp-sdk
    GIT_REPOSITORY https://github.com/jesspig/modelcontextprotocol-cpp-sdk.git
    GIT_TAG        0.2.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(mcp-cpp-sdk)

# ====================================================================
# googletest — 单元测试框架
# ====================================================================
if(GSD_BUILD_TESTS)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

message(STATUS "[gsd] Dependencies: godot-cpp + mcp-cpp-sdk")
