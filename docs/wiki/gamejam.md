---
type: 工作流指南
title: Game Jam 抽题与项目收集
description: 比赛开发与 e2e 用的 Game Jam 题库、随机抽样、intake 与项目创建、固定 e2e 沙盒、创意约束与本地收藏
tags:
  - Game Jam
  - e2e
  - 抽题
  - devtools
timestamp: "2026-11-17T02:19:35+08:00"
resource:
  - devtools/commands/jam.ts
  - devtools/data/gamejam/themes.json
  - devtools/templates/gamejam/
  - devtools/lib/jam_projects.ts
---

# Game Jam 抽题与项目收集

服务两类流程：比赛开发用 `gda-game-develop` 抽题、命名、创建并保留项目；端到端测试用 `gda-e2e-testing` 在固定沙盒 `examples/e2e` 中重置并开发，只记录工具问题。

程序化测试（ctest、`bun test`、单元/集成/守卫）不依赖抽题或 `examples/` 生成物；只有生成器自身的单测在临时目录覆盖它。

## 题库

数据文件：`devtools/data/gamejam/themes.json`。当前为 `schema_version: 2`，共 50 条。

| 字段 | 必填 | 说明 |
|---|---|---|
| `id` | 是 | 复合唯一键，同时作为主题目录名，如 `game_off_2026_shift` |
| `theme` | 是 | 展示主题；不得含双引号、反斜杠或换行 |
| `organizer` | 是 | 主办方，用于后续按主办方筛选 |
| `event` | 是 | 赛事名，如 `Game Off`、`Ludum Dare` |
| `edition` | 是 | 届次或年份标识，如 `2026`、`56`、`2026.2` |
| `year` | 是 | 四位年份，用于按年份筛选 |
| `source_url` | 是 | 真实来源；必须是 `http`/`https` |
| `source_title` | 否 | 来源页标题 |
| `source_kind` | 否 | `official` / `archive` / `community` |
| `rules_url` | 否 | 赛事规则页；仅作来源追溯，不渲染进任务书 |
| `verified_at` | 否 | 来源核验日期，`YYYY-MM-DD` |
| `tags` | 否 | 未来筛选用的标签，建议前缀 `genre:` / `mechanic:` / `mood:` / `input:` |
| `design_constraints` | 否 | 对游戏设计/内容的创意约束，字符串数组；无约束为 `[]` |
| `keywords` | 是 | 主题解读方向，至少 5 条 |
| `concept_ideas` | 是 | 概念灵感，至少 5 条 |

展示用来源名由 `event + edition` 派生，不单独存储。

来源策略：官方页优先；历史主题官方页下线时允许 Wikipedia 或 Web Archive，标 `source_kind: "archive"`；没有任何可验证来源的主题不入库。

**创意约束只收设计/内容层限制**：玩家数量、类型、内容分级、输入方式、画面/分辨率/配色、体积上限、必须包含或禁止的元素等会直接改变玩法或呈现的限制。赛期、团队人数、引擎、提交平台、源码公开、素材授权、AI 工具限制等运营或生产规则不写入 `design_constraints`；我们只用题库跑端到端测试，这些规则不产生实际约束。没有设计/内容层限制时写 `[]`。

赛事模板 `devtools/templates/gamejam/brief.md` 由用户固定维护，生成器与技能不得修改；生成时只替换模板中出现的占位符。当前模板未包含创意约束占位符，因此 `design_constraints` 仅保留在题库与 `jam.json`，不会渲染进 `GAMEJAM.md`。

## 随机抽样

每次抽题时：

- `keywords` 随机抽 3–5 条。
- `concept_ideas` 独立随机抽 3–5 条。
- 两条列表分别无放回抽样并打乱顺序。
- `GAMEJAM.md` 只渲染本次抽到的子集，并注明“不是全部”。
- 抽样快照写入 intake、比赛最终项目与 `examples/e2e/jam.json`，便于复盘。

同一主题重复抽到会得到不同 brief；intake 会被刷新，已创建项目保留自己的 `GAMEJAM.md` 快照。

## 目录结构

```text
examples/
  e2e/                               # 端到端固定沙盒：每轮重置，可整目录删除
    project.godot                    # config/name="e2e"
    GAMEJAM.md                       # 本轮抽样后的任务书
    jam.json                         # kind=e2e、主题、抽样快照、draw_id
    ...                              # 本轮开发的场景/脚本/素材
  <theme_id>/
    project/                         # 比赛 intake：只读题，不开发
      project.godot                  # config/name="project"
      GAMEJAM.md                     # 本次抽样后的任务书
      jam.json                       # 主题、来源、创意约束、抽样快照、draw_id
    <slug>-<hash8>/                  # 比赛最终项目
      project.godot                  # config/name="<项目名>"
      GAMEJAM.md                     # 从 intake 复制的快照
      jam.json                       # 最终项目元数据（含创意约束与抽样快照）
      ...
  INDEX.md                           # 比赛项目索引（由 jam index 生成）
```

- `<hash8>` = `sha256(<项目名> + "\n" + <ISO 时间戳>)` 的前 8 位十六进制；目标已存在时追加尝试序号重算。
- 作品说明文档按最终项目里的 `GAMEJAM.md` 提交要求准备，文件名、格式与内容结构由该模板决定；生成器与技能不额外规定。
- 最终目录名使用 ASCII slug；中文名通过 `--slug` 指定短名，完整名称写入 `project.godot` 与 `jam.json`。
- intake 与比赛最终项目都保留 `GAMEJAM.md`；最终项目里的那份是当轮抽样快照。
- `examples/e2e` 固定名称，不进入主题目录与 `examples/INDEX.md`；`jam.json.kind=e2e`，发现逻辑与索引会跳过它。

## 命令

```bash
# 抽题：生成/刷新 intake
bun run gda-dev-tools jam [--theme <id|主题名>] [--json]

# 创建比赛最终项目：复制 intake、改名、写元数据
bun run gda-dev-tools jam create --theme <theme_id> --name "<项目名>" [--slug <slug>] [--out <dir>] [--json]

# 创建/刷新固定 e2e 沙盒；--reset 清空重建
bun run gda-dev-tools jam e2e [--reset] [--theme <id|主题名>] [--json]

# 查看题库与已收集项目
bun run gda-dev-tools jam list [--projects] [--json]

# 刷新比赛项目索引
bun run gda-dev-tools jam index [--json]
```

- `--theme` 先匹配 `id`，再匹配唯一的 `theme` 展示名；同名歧义时报候选。
- 抽题不接 `--out`；`--out` 只用于 `jam create`，且目录已存在时报错，不覆盖。
- `jam create` 要求 intake 存在且 `jam.json.kind=intake`；缺失或主题不匹配时退出码 2。
- `jam e2e` 只操作 `examples/e2e`：不存在时创建，存在且 `kind=e2e` 时刷新，`--reset` 时清空重建；非 e2e 目录一律拒绝覆盖或删除。
- `jam --json`/`jam create --json`/`jam e2e --json` 的 stdout 是纯 JSON，错误走 stderr；退出码 0 成功 / 1 运行时或数据错误 / 2 用法错误。

## 自动发现

`devtools/lib/jam_projects.ts` 的 `discoverProjectsIn()` 递归扫描 `examples/`，跳过 `.git`、`.godot`、`addons`、`build`、`node_modules`，并跳过 `jam.json.kind=intake` 的 intake 与 `jam.json.kind=e2e` 的固定沙盒。

`devtools/lib/config.ts` 的 `discoverExampleProjects()` 调用它，因此 `deploy`/`clean` 未传 `--project` 时只看到比赛最终项目，不会把 intake 或 `examples/e2e` 当作可部署工程。

`used` 判定基于比赛最终项目：某个主题下至少存在一个 `kind=project` 才计入“已用”；intake 与 `kind=e2e` 都不算。

## 比赛开发流程（gda-game-develop）

1. `jam --json` 抽题，得到 `theme_id`、`intake_path`、`draw_id`。
2. 读 `intake_path/GAMEJAM.md` 与 `jam.json`；不要在 intake 中开发或打开 Godot。
3. 构思 2–3 个项目名与概念，选定展示名和 ASCII slug。
4. `jam create --theme <theme_id> --name "<项目名>" --slug "<slug>" --json` 生成比赛最终项目。
5. 立即执行 `build` + `deploy --project <project-dir>`；请用户在 Godot 中打开并启用插件，校验 `gda status --project <project-dir> --json`。
6. 按最终项目里的 `GAMEJAM.md` 提交要求准备作品说明；文件名、格式与内容结构以该模板为准，技能不额外规定。
7. 开发中遵守 `GAMEJAM.md` 的「创意约束」；它与模板建议冲突时以创意约束为准。
8. 收尾执行 `jam index`，刷新 `examples/INDEX.md`。

详细流程见 [gda-game-develop 技能](../../.agents/skills/gda-game-develop/SKILL.md)。

## 端到端测试流程（gda-e2e-testing）

1. 请用户关闭正打开 `examples/e2e` 的 Godot。
2. 每个新轮次先重置：`jam e2e --reset --json`；只有用户明确要求续跑时才跳过。
3. 读取 `examples/e2e/GAMEJAM.md` 与 `jam.json`。
4. 立即执行 `build` + `deploy --project examples/e2e`。
5. 请用户打开 `examples/e2e` 并启用插件；校验 `gda status --project examples/e2e --json`。
6. 按端到端测试技能推进，只记录工具问题，不做修复。
7. 报告写到 `report/<YYYY-MM-DD_HH-MM>/`，不执行 `jam index`；`examples/INDEX.md` 只收比赛项目。

详细流程见 [gda-e2e-testing 技能](../../.agents/skills/gda-e2e-testing/SKILL.md)。

## 本地收藏

`examples/` 默认不入库（`examples/.gitignore` 忽略全部生成物），项目只保留在本地。`jam index` 生成 `examples/INDEX.md`，包含比赛项目名、主题、赛事、年份、创建时间与工程路径；`examples/e2e` 不进入索引。

`jam list --projects` 提供同一批比赛项目的文本/JSON 视图，便于脚本处理。端到端沙盒用 `jam e2e` 管理，不参与收藏索引。

## 扩充题库

1. 在 `themes.json` 的 `themes[]` 追加一条 v2 记录。
2. 确认 `id` 唯一、`source_url` 可访问、`keywords`/`concept_ideas` 各至少 5 条。
3. 查找该赛事规则页，只把设计/内容层限制写入 `design_constraints`；时间、引擎、团队、提交平台、素材授权、AI 工具等规则不写入。没有明确限制时写 `[]`，不要编造。
4. 若来源是历史归档，标 `source_kind: "archive"`。
5. 运行 `bun test cli/tests` 与 `bun run gda-dev-tools jam --list` 验证。

单测见 `cli/tests/jam.test.ts` 与 `cli/tests/jam_projects.test.ts`；数值口径见 [tests.md](./tests.md)。
