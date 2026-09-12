# 构建期把 src/util/skill_templates/ 嵌入为生成头（勿手改 build/generated/ 下产物）
find_program(GDA_PYTHON_EXECUTABLE NAMES py python python3 REQUIRED)
execute_process(
    COMMAND "${GDA_PYTHON_EXECUTABLE}" --version
    RESULT_VARIABLE GDA_PYTHON_CHECK
    OUTPUT_QUIET ERROR_QUIET)
if(NOT GDA_PYTHON_CHECK EQUAL 0)
    message(FATAL_ERROR "[gda] Python 解释器不可用（GDA_PYTHON_EXECUTABLE=${GDA_PYTHON_EXECUTABLE}）。Windows 上若命中 Microsoft Store 存根，请安装官方 Python 或调整 PATH。")
endif()
set(GDA_SKILL_TEMPLATES_DIR "${CMAKE_SOURCE_DIR}/src/util/skill_templates")
set(GDA_SKILL_EMBED_HEADER "${CMAKE_BINARY_DIR}/generated/skill_content_embedded.h")

file(GLOB GDA_SKILL_TEMPLATE_FILES CONFIGURE_DEPENDS
    "${GDA_SKILL_TEMPLATES_DIR}/*.md")
list(APPEND GDA_SKILL_TEMPLATE_FILES "${GDA_SKILL_TEMPLATES_DIR}/registry.json")

add_custom_command(
    OUTPUT "${GDA_SKILL_EMBED_HEADER}"
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/embed_skills.py"
            --templates "${GDA_SKILL_TEMPLATES_DIR}"
            --output "${GDA_SKILL_EMBED_HEADER}"
    DEPENDS ${GDA_SKILL_TEMPLATE_FILES}
    COMMENT "[gda] Embedding skill templates"
    VERBATIM)

add_custom_target(gda_skill_embed_header DEPENDS "${GDA_SKILL_EMBED_HEADER}")
