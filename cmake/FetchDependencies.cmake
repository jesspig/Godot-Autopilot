# ====================================================================
# Godot-Autopilot 依赖管理 (FetchContent)
# 仅使用公开 GitHub 仓库，确保其他用户可直接构建。
# 第一次需要网络，之后缓存到 build/<preset>/_deps/。
# 禁止删除 _deps/，否则需要重新下载。
# ====================================================================
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# mcp-cpp-sdk 的 OpenSSL/TLS 为可选依赖，本插件 MCP 服务仅监听 127.0.0.1 明文 HTTP，
# 统一禁用以保证三平台产物一致（macOS homebrew 仅有 arm64 OpenSSL，universal 链接会失败）。
set(CMAKE_DISABLE_FIND_PACKAGE_OpenSSL ON)

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
    GIT_TAG        0.3.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(mcp-cpp-sdk)

message(STATUS "[gda] Dependencies: godot-cpp + mcp-cpp-sdk")
