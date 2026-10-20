# ====================================================================
# API 基座 — 契约分支源文件清单（变量 GDA_API_CONTRACT_SOURCES）
#
# 由并行子任务分支「API 契约」独占维护：新增编译单元只登记到本文件，
# 不改根 CMakeLists.txt，避免多分支归并冲突。
# ====================================================================
set(GDA_API_CONTRACT_SOURCES
    src/tools/tool_execute.cpp
    src/tools/argument_validator.cpp)
