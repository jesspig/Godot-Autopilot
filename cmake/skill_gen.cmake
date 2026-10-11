# 构建期把 skills/ 标准目录嵌入为生成头（勿手改 build/generated/ 下产物）
#
# find_package(Python3) 而非 find_program：它会真正执行解释器做验证，因此不会命中
# Microsoft Store 的 python 存根，且能识别 python3.12 这类带版本号的文件名；
# find_program(NAMES py python python3) 只认三个裸名字，uv/conda 等安装的
# 版本化可执行名会漏掉。godot-cpp 已先调用过 find_package(Python3)，此处复用其结果。
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    set(GDA_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
else()
    find_program(GDA_PYTHON_EXECUTABLE NAMES py python python3)
endif()
if(NOT GDA_PYTHON_EXECUTABLE)
    message(FATAL_ERROR "[gda] 未找到可用的 Python 解释器。请安装官方 Python 并确保 py/python/python3 在 PATH；或用 `uv run main.py build`（脚本会把当前解释器目录注入 PATH）。")
endif()
execute_process(
    COMMAND "${GDA_PYTHON_EXECUTABLE}" --version
    RESULT_VARIABLE GDA_PYTHON_CHECK
    OUTPUT_QUIET ERROR_QUIET)
if(NOT GDA_PYTHON_CHECK EQUAL 0)
    message(FATAL_ERROR "[gda] Python 解释器不可用（GDA_PYTHON_EXECUTABLE=${GDA_PYTHON_EXECUTABLE}）。Windows 上若命中 Microsoft Store 存根，请安装官方 Python 或调整 PATH。")
endif()
set(GDA_SKILLS_DIR "${CMAKE_SOURCE_DIR}/skills")
set(GDA_SKILL_EMBED_HEADER "${CMAKE_BINARY_DIR}/generated/skill_content_embedded.h")

file(GLOB GDA_SKILL_FILES CONFIGURE_DEPENDS
    "${GDA_SKILLS_DIR}/*/SKILL.md"
    "${GDA_SKILLS_DIR}/*/references/*.md"
    "${GDA_SKILLS_DIR}/*/scripts/*.mjs")

add_custom_command(
    OUTPUT "${GDA_SKILL_EMBED_HEADER}"
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/embed_skills.py"
            --skills "${GDA_SKILLS_DIR}"
            --output "${GDA_SKILL_EMBED_HEADER}"
    DEPENDS ${GDA_SKILL_FILES}
    COMMENT "[gda] Embedding skills"
    VERBATIM)

add_custom_target(gda_skill_embed_header DEPENDS "${GDA_SKILL_EMBED_HEADER}")
