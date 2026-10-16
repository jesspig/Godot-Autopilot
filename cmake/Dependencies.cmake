# ====================================================================
# Godot-Autopilot 依赖管理 (git submodule)
#
# godot-cpp / mcp-cpp-sdk / googletest 均以 submodule 置于 third_party/,
# 版本由 gitlink 锁定的 commit 唯一确定, 不随上游 tag 移动而漂移。
#
# 选 submodule 而非 FetchContent 的理由:
#   - third_party/ 在 build/ 之外, 不被 .gitignore 排除, 随仓库一同流转;
#     FetchContent 的缓存位于 build/<preset>/_deps/, 换机或内网克隆后不存在,
#     必须重新访问 GitHub —— 在无外网或 GitHub 不可达的环境下直接构建失败。
#   - submodule checkout 到位后, configure 阶段完全不产生任何网络访问。
#
# 检出方式: git clone --recurse-submodules <repo>
#           或事后 git submodule update --init --recursive
#
# 缺失时直接 FATAL_ERROR 而非回退网络拉取: 回退会让构建行为不确定,
# 内网环境下会在 configure 中途抛出难以定位的 git 报错。
# ====================================================================

set(GDA_THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")
set(GDA_SUBMODULE_BIN_DIR "${CMAKE_BINARY_DIR}/_sub")

function(gda_require_submodule name)
    if(NOT EXISTS "${GDA_THIRD_PARTY_DIR}/${name}/CMakeLists.txt")
        message(FATAL_ERROR
            "[gda] third_party/${name} 缺失。请先初始化子模块:\n"
            "  git submodule update --init --recursive\n"
            "或重新克隆时加上 --recurse-submodules。")
    endif()
endfunction()

# mcp-cpp-sdk 的 OpenSSL/TLS 为可选依赖，本插件 MCP 服务仅监听 127.0.0.1 明文 HTTP，
# 统一禁用以保证三平台产物一致（macOS homebrew 仅有 arm64 OpenSSL，universal 链接会失败）。
set(CMAKE_DISABLE_FIND_PACKAGE_OpenSSL ON)

# ====================================================================
# godot-cpp — Godot Engine C++ 绑定层 (10.0.0-stable / 507ed9d)
# ====================================================================
gda_require_submodule(godot-cpp)
set(GODOTCPP_API_VERSION "4.7" CACHE STRING "The Godot API version to target" FORCE)
add_subdirectory("${GDA_THIRD_PARTY_DIR}/godot-cpp" "${GDA_SUBMODULE_BIN_DIR}/godot-cpp")

# ====================================================================
# mcp-cpp-sdk — MCP 协议 C++ SDK (0.3.7 / 7dfe088)
# ====================================================================
gda_require_submodule(mcp-cpp-sdk)
add_subdirectory("${GDA_THIRD_PARTY_DIR}/mcp-cpp-sdk" "${GDA_SUBMODULE_BIN_DIR}/mcp-cpp-sdk")

message(STATUS "[gda] Dependencies: godot-cpp + mcp-cpp-sdk (third_party submodule)")
