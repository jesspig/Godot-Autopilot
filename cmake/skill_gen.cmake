# 构建期把 skills/ 标准目录嵌入为生成头（勿手改 build/generated/ 下产物）
#
# 生成器为仓库内 tools/embed_skills.ts，由 Bun 直接执行。Bun 缺失或不可用时
# configure 直接失败并给出安装提示，避免到构建阶段才暴露。
find_program(GDA_BUN_EXECUTABLE NAMES bun)
if(NOT GDA_BUN_EXECUTABLE)
    message(FATAL_ERROR "[gda] 未找到 Bun。请安装 Bun 1.4+（https://bun.sh）并确保 bun 在 PATH；CI 由 oven-sh/setup-bun@v2 提供。")
endif()
execute_process(
    COMMAND "${GDA_BUN_EXECUTABLE}" --version
    RESULT_VARIABLE GDA_BUN_CHECK
    OUTPUT_QUIET ERROR_QUIET)
if(NOT GDA_BUN_CHECK EQUAL 0)
    message(FATAL_ERROR "[gda] Bun 不可用（GDA_BUN_EXECUTABLE=${GDA_BUN_EXECUTABLE}）。请重新安装 Bun 并确保 bun --version 可执行。")
endif()
set(GDA_SKILLS_DIR "${CMAKE_SOURCE_DIR}/skills")
set(GDA_SKILL_EMBED_HEADER "${CMAKE_BINARY_DIR}/generated/skill_content_embedded.h")

file(GLOB GDA_SKILL_FILES CONFIGURE_DEPENDS
    "${GDA_SKILLS_DIR}/*/SKILL.md"
    "${GDA_SKILLS_DIR}/*/references/*.md"
    "${GDA_SKILLS_DIR}/*/scripts/*.mjs")

add_custom_command(
    OUTPUT "${GDA_SKILL_EMBED_HEADER}"
    COMMAND "${GDA_BUN_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/embed_skills.ts"
            --skills "${GDA_SKILLS_DIR}"
            --output "${GDA_SKILL_EMBED_HEADER}"
    DEPENDS ${GDA_SKILL_FILES}
    COMMENT "[gda] Embedding skills"
    VERBATIM)

add_custom_target(gda_skill_embed_header DEPENDS "${GDA_SKILL_EMBED_HEADER}")
