---
type: 模块文档
title: 技能卡与 references
description: CLI-only 技能书目录、入口卡契约、8 个 recipe 的八段结构、加载预算、构建管线与守卫规则
tags:
  - 模块
  - 技能
  - references
  - CLI
  - 守卫
timestamp: "2026-11-26T09:00:00+08:00"
resource:
  - skills/
  - cli/scripts/build-skills-asset.ts
  - tools/embed_skills.ts
  - tests/guard/skills_guard.ts
---

# 技能卡与 references（`skills/`）

> 当前状态：守卫检查结构、命令、flag、JSON 与预算，不描述 MCP 协议。

## 目录结构

一本技能书 = `godot-autopilot/SKILL.md` + `references/**/*.md`。

- `SKILL.md` frontmatter 只允许 `name` 与 `description`；`references/` 不带 frontmatter，首行为 `#` 标题。
- 守卫拒绝除 `SKILL.md` 与 `references/*.md`（含 `references/recipes/*.md`）之外的任何路径。
- T-13 的目标结构：`SKILL.md`；`references/` 下 `setup.md`、`cli.md`、`safety.md`、`offline.md`、`scene.md`、`scripting.md`、`resources.md`、`runtime.md`、`debugging.md`、`testing.md`、`content.md`、`csharp.md`、`servers.md`、`workflows.md`；`references/recipes/` 下 8 个 recipe。
- 入口卡承担 CLI-only 启动、动态发现、确认、错误恢复与一级指令路由；领域细节下沉到 references。

## 入口卡 CLI-only 契约

`SKILL.md` 必须同时满足：

- CLI-only 词表（守卫 token 表见 `tests/guard/skills_guard.ts`）：不出现 `MCP`、`Model Context Protocol` 等旧协议词、旧端点配置 token 与 `godot://skills`、`search_tools`、`get_tool_detail`、`call_tool`、`list_categories`、`resources/list`、`resources/read`、`prompts/list`、`prompts/get`、`tasks/get`、`tasks/cancel`、`resultType`、`taskId` 与 `--direct`、`--transport`、`--no-fallback`、`--input`、`--async`。
- CLI-only 健康探测：`gda ping`（或 `gda tool ping`），只描述为健康探测，不是 handshake。
- 动态发现：`gda tool list`、`gda tool search`、`gda tool describe`；通用逃生口 `gda tool call`。工具名、数量与 schema 一律现场查询，不写死。
- 确认：`--confirm` 与 `confirmation_required`；结果未知不重发。
- 错误恢复：`gda doctor`、`gda job`。
- 一级指令路由：至少 3 行指向 `references/` 的表格路由行。

## Recipe 契约

固定 8 个 recipe，路径 `references/recipes/<id>.md`，id 为 `bootstrap`、`scene-script`、`asset-pipeline`、`playtest-debug`、`safe-refactor`、`error-recovery`、`skills-sync`、`long-task`。

- 每个 recipe 首行标题必须是 `# <id>`，与文件名同构。
- 每个 recipe 必须按序包含 `## 1.` 到 `## 8.` 八个编号小节，依次对应：触发条件与输入、前置检查、可复制的 CLI 命令、每步观察字段、验证与退出条件、失败分支、安全与确认点、明确不做什么。
- recipe 内命令必须存在于 CLI 契约；JSON 示例按 envelope/action schema 校验。

## 命令、flag 与 JSON 契约

- 命令与局部 flag 由 `cli/src/command-spec.ts`（`COMMAND_GROUPS`、`COMMAND_SPECS`、`LEGACY_ALIASES`、`FLAG_MIGRATIONS`）与 `cli/src/args.ts` 派生；守卫校验完整 `gda <group> <subcommand>` 与命令局部 flag，不再只校验顶层 `gda`。
- 已迁移 flag（`--all`、`--force`、`--file`、`--probe` 等）分类为 `migrated-flag`，并给出替代 flag 提示；MCP 时代 flag 分类为 `mcp`。`skills_contract.test.ts` 暂过滤这两类，T-13 重写后移除过滤恢复严格断言。
- fenced JSON 必须可解析且为对象；key 限定为 `cli/src/output.ts` 的 envelope 控制键与工具参数 schema 键；含 `action` 时必须是非空字符串且在 CLI/工具 action 集合内。
- 禁止易漂移事实：工具数量 `252`/`253`、别名数量 `325`、版本窗口 `0.4.x`/`0.6.x`/`0.4.0`/`0.6.0`，以及完整旧名→canonical 迁移表（表头含 alias/old/legacy/deprecated/canonical/migration 且带 Markdown 分隔行即命中）。


## 加载预算

按文件实际空白分词统计，不在技能正文固化数值；守卫常量在 `tests/guard/skills_guard.ts`。

| 项 | 上限 |
|---|---:|
| 单文件 | 2,000 tokens |
| 全量加载 | 24,000 tokens |

超预算先拆重复段落，再删低价值示例；不得删安全段。

## 构建管线

- `bun run build:skills-asset` 执行 [`cli/scripts/build-skills-asset.ts`](../../../cli/scripts/build-skills-asset.ts)，生成 `cli/assets/skills.json`：`{schema_version, version, file_count, content_digest, files, retired}`。
- `schema_version` 固定为 1；`file_count` 是文件数；`content_digest` 是版本与规范化文件树的 SHA-256，安装前复算，不匹配即拒绝安装。
- `retired` 是退役目录名数组，供安装器精确清理；不参与内容嵌入。
- 构建期 [`cmake/skill_gen.cmake`](../../../cmake/skill_gen.cmake) 调用 [`tools/embed_skills.ts`](../../../tools/embed_skills.ts)，把 `skills/` 嵌入 `build/generated/skill_content_embedded.h`；`SKILL_COUNT=1`。
- 两个生成物都由脚本产出，禁止手工编辑；改内容后重新运行构建脚本或重新 configure。

## 安装、升级同步与回滚

- 安装入口 `install skills` / `skill install` 与 `update skills` 共用受管同步：`cli/src/skills.ts` 计算旧清单与新清单 diff，`cli/src/archive.ts` 的 `applyManagedTree` 负责逐文件临时文件写入 + 原子替换，失败按备份反向恢复并清理 `*.tmp`。
- 安装目录内的 `.gda-skills-manifest.json` 记录实际使用的 `version`、`content_digest`、`tree_digest` 与受管文件清单；安装报告输出真实 asset 版本与 digest，不回报 CLI 版本或缓存假版本。
- 删除范围仅限同一受管书内、以 `.md` 结尾且不在新清单中的旧 reference；跨技能书、清单外路径、`..` 逃逸与非文件路径一律拒绝。清单外的顶层目录（用户自建技能）只列入 `preserved`，永不删除。
- `retired` 目录只在 `--prune`（sync 的 `--force` 亦隐含）下清理，先原子改名到临时备份，全部写盘成功后删除备份；失败恢复原目录。
- 内容 digest 损坏、旧清单 schema 非法或越界声明时拒绝同步；`--force` 可重建受管文件与清单，但不会碰清单外目录。
- CLI 预演：`gda skills sync --check`（等价于子动作 `--dry-run`），输出 `would_delete` / `would_overwrite` / `preserved`，全程只读；强制写入用 `--force-install`（等价于子动作 `--force`）。
- 当前 `skills sync` 作为 `skills` 命令的子动作实现；`command-spec.ts` 与 `args.ts` 中一等公民的 `gda skill sync --dry-run` / `--force` 注册待补（见 T-14 遗留项）。

## Guard 与测试

- [`tests/guard/skills_guard.ts`](../../../tests/guard/skills_guard.ts)：`--self-test` 验证负例样本逐条被拒；普通模式对 `skills/` 全量检查并校验 asset 漂移。
- [`cli/tests/skills_contract.test.ts`](../../../cli/tests/skills_contract.test.ts)：临时过滤已迁移 flag 的 `migrated-flag` 与带迁移 token 的 `mcp` 违规，其余违规必须为空。
- [`tests/unit/skill_gen_test.cpp`](../../../tests/unit/skill_gen_test.cpp)：纯 `ToolCatalog`/ToolSpec 构造 fixture，不依赖 MCP SDK；校验零 MCP、零漂移事实、入口卡 CLI-only、recipe 八段与反引号 action 回验。
- [`tests/unit/skill_resources_test.cpp`](../../../tests/unit/skill_resources_test.cpp)：仅测试 `godot://skills` URI 解析与内容解析，不使用 MCP SDK fixture。
- 常用验证：`bun tests/guard/skills_guard.ts --self-test`、`bun tests/guard/skills_guard.ts`、`ctest --preset debug -R "Skill|skills_guard"`、`bun test cli/tests`。

## 维护步骤（改结构时）

1. 修改 `skills/`，保持相对路径、recipe ID、章节编号、命令顺序、JSON key 与 action 一致。
2. 结构或 recipe 数量变化时，同步 `tools/embed_skills.ts` 的 `SKILL_COUNT`、`tests/unit/skill_gen_test.cpp` 的 `kSkillFileCount` 与 `kRecipeIds`、`tests/guard/skills_guard.ts` 的 `RECIPE_IDS` 与预算常量。
3. 退役目录变化时改 `cli/scripts/build-skills-asset.ts` 的 `RETIRED_SKILLS`，并重新生成 asset。
4. 依次运行 `bun run build:skills-asset`、守卫 `--self-test`、守卫、生成器、CLI 技能测试与 ctest 技能子集。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| `mcp` / `migrated-flag` | 正文仍含旧协议词或已迁移 flag | 已改为 CLI-only 写法（守卫保留历史分类键） |
| `drift-count` / `drift-version` / `drift-alias-table` | 固化数量、版本窗口或迁移表 | 改为动态发现规则 |
| `entry-cli-only` / `entry-routing` | 入口卡缺启动/发现/确认/恢复/路由 | 补齐入口卡契约 |
| `recipe-missing` / `recipe-id` / `recipe-sections` | recipe 缺失、ID 不匹配或少段 | 按八段结构重排 |
| `json-key` / `json-action` / `json-parse` | JSON 示例不合 schema | 按 envelope/action schema 修正 |
| `budget-file` / `budget-total` | 加载预算超限 | 拆重复、删低价值示例，不动安全段 |
| `asset-drift` | 改了内容但没重生成 asset | `bun run build:skills-asset` |
