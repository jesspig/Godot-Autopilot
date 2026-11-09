---
type: 验收清单
title: 引擎行为验收清单
description: 已删除 L2 引擎轨 48 份用例蒸馏出的引擎行为验收点（按工具域分组）、历史事故回归点与 A 类迁移对照表；验收执行走 gda-e2e-testing 技能
tags:
  - 测试
  - 验收
  - e2e
  - L2蒸馏
timestamp: "2026-11-09T02:46:53+08:00"
resource: src/tools/
---

# 引擎行为验收清单

> **随工具面演进维护，数值不设硬口径。** 本页收录已删除的 L2 引擎轨（48 份 `tests/config/*.json`，提交 `c023a5c` 删除）蒸馏出的**引擎触达行为验收点**，供 Agent e2e 验收时对照。本页条目随工具面增删改同步演进；页内数值只作行为描述、不作断言钉，测试计数引用 [tests.md](tests.md)「数值核算总表」口径。**验收执行走 [gda-e2e-testing 技能](../../.agents/skills/gda-e2e-testing/SKILL.md)**（衔接说明见页尾，技能流程零改动）。

## 用途与边界

- **为什么有本页**：48 份 L2 用例删除后，其覆盖的「必须真实编辑器/游戏进程才能验收」的行为语义不能丢——进程内可复现的契约面已由集成层 109 项钉住（[tests.md](tests.md)「集成层」），本页承载**划回 e2e 的引擎触达面**，逐域列出行为点并回溯来源。
- **来源纪律**：每条行为点标注来源（用例编号或历史事故的 changelog/git 记录），无来源的条目不收；蒸馏自 git 历史（`git show c023a5c~1:tests/config/NN_*.json`）、旧版 tests.md 用例表（`0dad560~2`）与 `.handoff/evidence/` 四份迁移对照表。
- **域覆盖口径**：本页只列 48 份用例覆盖到的工具域；未覆盖的域（physics/nav/audio/render/display/os/text/animation/gridmap/tween/analyze/test/debug 等）不因缺页而失去行为——它们的行为面由工具自述与 [modules/tools_ops_a.md](modules/tools_ops_a.md)、[modules/tools_ops_b.md](modules/tools_ops_b.md) 承载。
- **前置标注**：依赖 .NET 编辑器、游戏进程、GUI 渲染等前置的条目在所属域内标注；集成层已覆盖的参数校验错误串不重复收录。

## 域索引与 48 用例映射

| 域 | 对应实现 | 48 用例来源 |
|---|---|---|
| 1. 元工具与工具面契约 | 元工具（register_all）、工具注册/排除面 | 00、03、40、42 |
| 2. 授权门 | authorization（默认拒绝/注入语义） | 47（35/16/23/37 为授权前置态） |
| 3. 场景节点与属性 | scene_ops、property_ops、scene_spec_ops、spriteframes_ops | 01、02、08、13、20、28、32、34、42（写入链路）、15（写回显） |
| 4. 编辑器会话与场景文件 | editor_ops（打开/保存/脏标记/核验） | 07、25、33、43、15 |
| 5. 编辑器 UI 与输入 | editor_ui_ops、editor_ui_actions、input_ops、input_map_ops | 09、10、11、24（编辑器侧） |
| 6. 截图与视觉辅助 | capture_ops | 12、17、48 |
| 7. 资源与引用事务 | resource_ops、theme_ops | 04、05、06、22、31（theme 落盘） |
| 8. 脚本与 GDScript 执行 | script_ops、code_exec_ops | 04、18、19、31、39、26/27（宿主） |
| 9. 瓦片地图 | tilemap_ops、tileset_ops | 14 |
| 10. 游戏运行时通道 | runtime_ops、runtime_game_ops、scene_tree_ops、debugger_ops | 16、23、24（游戏侧）、35、37、45、46 |
| 11. C# 与 .NET | csharp_bridge_ops、C# 工具链（text/file 域的 C# 工具） | 38、44 |
| 12. 动态用户工具 | dynamic_spec_store（AutopilotTools 脚本 API） | 26、27 |
| 13. 可观测性与主线程韧性 | monitor/trace/log 持久化、CommandQueue/dispatch | 29、30、36、41 |

48 份用例（编号 00–48，缺 21 号）全部落入上表；一例跨两域时次要域以括注标明。

## 域 1：元工具与工具面契约

- ping 返回 pong（`00_meta.json`）。
- search_tools：空 query 枚举 catalog 全量、每项含 name 与 score；关键词、分类、标签三查均返回非空 results（`00_meta.json`、`03_tools_contract.json` 工具面契约）。
- list_categories 返回非空 categories（`00_meta.json`）。
- get_tool_detail：命中返回 `tool.*` 形状且 `input_schema` 为对象；未知名返回 `tool not found`；缺 name 报 `missing required parameter`（`00_meta.json`）。对运行时枚举的全部工具，`side_effect`/`mutating`/`dynamic` 字段存在且类型可读（`03_tools_contract.json`，集成层 toolface 契约承接）。
- `side_effect` 序列化为枚举名、机器可读：`code_execute` 工具的 detail 返回 `tool.side_effect == "code_execute"`（`40_contract_assert.json`）。
- call_tool：未知域工具报 `domain tool ... not found`；缺 name 报缺参（含 `received no parameters` 变体）（`00_meta.json`）。
- batch_execute：逐项执行并返回 succeeded/failed 计数，单项失败不掩盖其余（`00_meta.json`）。
- 排除划分自洽：`side_effect` 非 none、`mutating`、`dynamic` 任一命中即退出遍历候选；候选 ∩ 排除 = ∅ 且并集覆盖全集（`03_tools_contract.json`，动态枚举复算）。
- **回归点（Example 污染事故，changelog/2026-09-18-log.md，提交 `ca1461c` 考古）**：`set_editor_main_scene` 曾把 `application/run/main_scene` 写成 `"test"` 写入 `Example/project.godot`、`save_editor_scene` 空参生成 `Example/NewNode.tscn`——新增写配置/写文件/弹窗/改窗口的工具必须在 ToolSpec 声明对应 `side_effect`（必要时 + `kMutating`），否则全量遍历会污染用户工程或桌面。
- **回归点（[input] 段污染，changelog/2026-09-29-log.md）**：`add_input_map_action` 声明 `None` 未被遍历排除，启发式冒烟把 `[input] test=…` 写入床 `project.godot`——同一声明纪律的实证。
- 空参必报缺参：schema `required` 非空但空参未报错记 warnings；历史上 4 个工具（`start_input_gamepad_vibration` / `stop_input_gamepad_vibration` / `get_resource_extensions` / `reimport_resource_files`）的缺口已于 2026-10-21 修复（`03_tools_contract.json` warnings 基线语义，旧 tests.md「warnings 语义」节）。
- **回归点（遍历豁免与改判，changelog/2026-10-19 至 10-22-log.md）**：`create_scene_node` 由 `None` 改判 `writes_scene` 退出遍历（空参不再意外建根，连带 `intersect_physics_2d_ray` 的 `could not resolve 2D physics space: …` 文案进入预期错误豁免表）——工具声明变更必须同步核对遍历候选与豁免表。
- json 参数类型接受任意 JSON：property_set 直写标量/布尔/对象/数组不再被类型校验拒绝（`42_tool_contract.json`）。
- e2e 抽查建议：跑一轮 demo 时顺带用 search_tools/describe 核对新增工具的 `side_effect` 与 schema 是否如实声明；发现"空参不报错"或"未声明副作用"即登记。

## 域 2：授权门

- 默认拒绝态（不声明 requires 时执行器注入 `GODOT_AUTOPILOT_ALLOW=none` 哨兵；e2e 对应「未授权能力不注入」）：write 档工具在授权门（`deny_if_unauthorized`，先于 handler 参数校验）即被拒，响应为结构化拒绝形状——`error` 为 authorization required 精确全串、`code=authorization_required`、`authorization_required=write`（`47_authorization_gate.json`，集成层 `authorization_gate_test.cpp` 已逐字钉住进程内可达部分）。
- 授权态与通道态可区分：同一工具在「未授权」与「已授权但无游戏」两种态下错误文案不同，验收时须分辨（`35_scene_tree_gate.json` 两态口径）。
- 历史缺口注记：授权拒绝路径此前从未被自动断言（42 号仅手工验证 REST 403），47 号补上自动断言缺口（`47_authorization_gate.json` description 自述）。
- e2e 抽查建议：e2e 环境按需注入能力组合（面板/env/配置三来源）；遇到 authorization required 时先核对本轮授权姿态再判缺陷。

## 域 3：场景节点与属性

- 建节点：显式根路径建节点与相对 path 断言、省略 parent_path 在已有根时报双根错误、非法类型报错、子节点挂载（`01_scene.json`；「省略 parent_path 建根」语义由双根错误覆盖）。
- 删节点：返回 deleted 且 undo 动作存在；queue_free 为异步释放（同步断言树中消失不可行）；根节点禁删；删缺失节点报错（`01_scene.json`）。
- property_get/set/get_list：返回结构、错误措辞、readback MATCHED、int 字符串静默转 0、缺 value 报错（`02_property.json`）。
- Node 引用属性：路径字符串自动转换 + `converted_node_path` 回执，坏路径报错；typed 数组元素级转换（`Array[Node2D]` 接受路径字符串与 `{"__node_ref__": 路径}`，`Array[PackedScene]` 接受 `res://` 路径；坏元素与非数组值报错）；GDScript 全局类数组（`Array[GdaPropItem]`）落盘引用回读，`memory://` 禁入节点防坏场景文件（`02_property.json`）。
- 严格 JSON 形状回读（VariantJson::deserialize_strict 的引擎侧成功路径）：Rect2 文档形状 `{size:{w,h}}` 与别名 `{x,y}`（同现冲突报 `size.x and size.w conflict`）、position 可省、Transform2D 需 columns、Vector2/Vector2i 一律要求对象形状（数组形状报 `invalid Vector2: expected a JSON object`）；create_scene_node 的 `applied_properties` 精确匹配并读回证明已应用（`08_property_readback.json`；形状层由 L1 variant_json_strict_test 钉住）。
- 内联子资源一步创建：property_set 与 create_scene_node 的 properties 内联 `{"type": ..., "properties": {...}}` 建 `[sub_resource]`，嵌套 depth 2 成功、5 层报 nesting limit；未知类名/非 Resource 类/抽象类/properties 非对象报错且报错后原属性值不变；`{"path": "res://..."}` 磁盘资源与 `memory://` 拒绝文案各一条，导出 Dictionary 属性写入同形对象仍按普通字典存值（防误伤）（`13_inline_subresource.json`；解析层由 L1 inline_resource_json_test 钉住）。
- **回归点（B2，dry_run 假校验事故）**：`build_nodes_from_spec` 的 `dry_run` 必须与真实写入同路径同结论——OBJECT 型内联资源走真实构建 + 严格校验；WorldBoundaryShape3D.plane 四元形状真实写入读回，`dry_run` 缺 d / 零法线 / 未知内联类三负例与正例（`mode=dry_run`、`trial_*` 计数、零写入）精确断言（`32_plane_inline.json`）。
- **回归点（P2-2，属性两轮应用顺序事故）**：create_scene_node 同传 `sprite_frames` + `animation` 时资源类属性必须先应用（否则引擎 set_animation 把动画名清空）；断言 `applied_properties == ["sprite_frames","animation"]` 且读回 `animation == "idle"`（夹具经 create_spriteframes/add_spriteframes_animation 构建后 save_resource 落盘）（`28_sprite_frames_animation.json`）。
- **回归点（B5，斜杠属性拒绝事故）**：引擎原生斜杠属性名（`theme_override_colors/font_color`）直写读回；非精确名 + 同前缀族的 `expanded_from` 批量展开与非法键报错；`build_nodes_from_spec` dry_run 同口径；点号深路径（`material.albedo_color`，Object 中段）回归（`34_slash_property.json`）。
- 中文文本往返：property_set 写中文到 Label.text、property_get 读回逐字相等（String UTF-8 链路）（`20_cjk_text_roundtrip.json`）。
- 写入真实生效：property_set 依次写标量/布尔/对象/数组后 property_get 逐项回读（`42_tool_contract.json`）。
- e2e 抽查建议：建节点 + 设一组含内联子资源/斜杠属性/typed 数组的属性后读回比对，是覆盖本域回归点（B2/B5/P2-2）效率最高的组合动作。

## 域 4：编辑器会话与场景文件

- 场景脏标签闭环：property_set 致脏后 open 其他场景被拒并提示 save_editor_scene 或 reload_editor_scene；reload_editor_scene 返回 reloaded/scene_path/observed 清脏后 open 成功（`07_scene_tabs.json`）。
- reload_editor_scene 对未打开场景报 `scene is not open`；create_editor_scene 的 timeout_ms 非整数/越界报错且不影响当前场景（`07_scene_tabs.json`）。幽灵标签分支（多标签共存时移动依赖）历史未覆盖。
- **回归点（open_editor_scene root 不变误报）**：目标场景已打开时二次打开必须命中幂等分支返回 `result=ok / already_open=true`（旧实现以 root 指针变化判定成败而误报）；无效路径仍报错不静默成功；`reload_if_open=true` 从磁盘重建场景树，verify_scene_saved 证明重载前 mismatch、重载后 match（`25_open_scene_idempotent.json`）。
- **回归点（B4，含子场景实例的保存核验误判）**：实例内部继承节点（owner 非当前场景根、不落盘）不计入 memory/disk 对比；save_editor_scene_as 回执 `nodes_match=true` 且 `inherited_nodes` 分离计数；verify_scene_saved 的 `inherited_paths` 精确列出继承路径（`33_verified_inherit.json`）。
- scene_unsaved 为真实脏标记（来自 `EditorInterface.get_unsaved_scenes`，非"有无磁盘路径"）：property_set 后响应与快照均报 true、save 后回 false（`43_scene_integrity.json`、`15_scene_path.json`）。
- **回归点（P3-2，未切换场景静默误建）**：编辑器写工具（create_scene_node/rename_scene_node/property_set/delete_scene_node/attach_script_to_node）响应回显 `scene_path`/`scene_unsaved`，未落盘场景上误建可立刻发现（`15_scene_path.json`）。
- verify_scene_saved 连接统计：persist=false 的运行时连接不计入持久连接（memory=disk=0）；persist=true 持久连接不被漏计（memory=disk=1）；全部编辑器写路径都正确置脏——signal_connect/signal_disconnect、set_script_property、build_nodes_from_spec、spawn_node_bundle 之后 get_scene_snapshot 均报 `scene_unsaved=true`、save 后回 false（`43_scene_integrity.json`）。
- **回归点（环境漂移，changelog/2026-09-26-log.md、2026-09-29-log.md）**：工程 `project.godot` 含 `run/main_scene` 且 `.godot/editor` 缓存记录最近场景时，编辑器启动会异步恢复主场景覆盖用例夹具；`.godot/editor/editor_layout.cfg` 的 `open_scenes` 残留指向已删路径会让下次启动报缺失——e2e 环境批量异常先查这两处生成物再怀疑插件。
- e2e 抽查建议：多场景轮换（建→脏→切→reload→落盘→核验）一个序列即可覆盖脏标签/幂等/B4/P3-2 四个回归点。

## 域 5：编辑器 UI 与输入

- get_editor_ui_elements 列表与 `space=window` 标记、get_editor_viewport_geometry 映射、hit_test_editor_point 命中链、get_display_window_rect 窗口矩形（`09_editor_ui.json`；headless 下元素数不定，只断言结构字段存在）。
- get_scene_node_screen_rect 缺 paths 报 `missing required parameter: paths (array)`（`09_editor_ui.json`）。
- scene_tree_items 行结构（path/name/type/depth/selected/collapsed/visible/rect）与 filter 大小写不敏感匹配、selected_only、max_items 截断（truncated）；select_scene_tree_node 默认清空并选中、联动 Inspector（inspect/focus/add 追加、selected_count），选中结果对 get_editor_selection 可见、选根节点替换选择并跳过 inspect；节点不存在返回含 position/expected/action 的结构化错误（`11_editor_tree.json`；focused 仅非 headless 为 true，headless 下返回 false 属正确行为）。
- 编辑器输入/UI 动作参数校验（集成层 `editor_input_validation_test.cpp` 已钉住错误串）：全部在注入 warp/事件之前返回 error，不产生真实点击/拖拽/滚轮/文本输入副作用（`10_editor_input.json`）。
- 键名口径统一：裸名 `P` 与 `KEY_P`、大小写归一、裸数字、`SPACE` 特殊键等价；未知名报带可用形式说明的 `unknown key name`（`24_keycode_alias.json` 编辑器侧 9 步；判决纯函数由 L1 keycode_alias_test 全量覆盖，handler 入口接线由源码背书）。
- InputMap 动作增删改查与持久化（`24_keycode_alias.json`；历史注记：其持久化曾经启发式冒烟落盘 [input] 段，见域 1 回归点）。
- **覆盖缺口（移交 e2e）**：`click_editor_element` / `type_editor_element_text` 的真实点击/文本输入从未纳入自动用例（旧 runner 不支持跨步骤变量引用），属编辑器 UI 动作自动化缺口（旧 tests.md「已知遗留」）。
- e2e 抽查建议：在 GUI 编辑器中做一次"枚举 UI 元素 → 按元素点击 → 场景树选中并确认 Inspector 联动"链路，补上历史未自动化的真实点击路径。

## 域 6：截图与视觉辅助

- capture 参数校验（集成层 `capture_params_validation_test.cpp` 已钉住错误串）：`after_frames` 负数/非整数、`when` 仅限 target=game、`scale` 越界、`save`/`save_dir` 类型错误、`res://../` 逃逸与 `user://` 拒绝（`12_capture_params.json`）。
- 工程外绝对路径拒绝（`path rejected: absolute path is outside the project root` 语义）：进程内不可复现（ProjectSettings 单例边界），错误串等价性由源码背书，验收在真实编辑器中发生（`12_capture_params.json` 的 editor_save_dir_outside_project 步，T-04 R-04-01）。
- 视觉辅助参数（集成层 `vision_assist_validation_test.cpp` 已钉住）：`annotate_nodes`/`annotate_nodes_max`/`diff_image`/`review_scene_visually` 全参数负例（`17_vision_assist.json`）。
- `diff_image requires diff_against_last` 前置校验：进程内不可复现（EditorInterface 单例边界），真实编辑器中验收（`17_vision_assist.json` 的 diff_image_requires_diff 步，T-04 R-04-02）。
- 帧相位与落盘（`48_capture_frame_phase.json`）：`when` 表达式命中抓拍返回 `when_satisfied_frame`/`captured_frame` 两帧号字段且 `captured_frame > when_satisfied_frame`；`after_frames` 延迟抓拍；连续 10 次 save 落盘互不相同的 `saved_path` 并经 list_directory 清单核对；`capture_editor_viewport(target=game)` 的 save 落盘（修复 target=game 静默忽略 save 的回归）。
- **前置：`48` 号帧相位与落盘语义必须 GUI 游戏渲染**（headless 游戏进程无渲染输出，无法取图；该用例曾因此临时改写测试床 `main_run_args` 并列入 isolated 名单）。`12`/`17` 的参数校验 headless 即可。
- e2e 抽查建议：GUI 会话中做一次 when/after_frames 抓拍与 save 落盘清单核对，顺带核对 capture 目录无越权路径写入。

## 域 7：资源与引用事务

- save_resource copy-on-write：按 path 加载→内存修改→存 dest_path，断言 copy_on_write/source_path、源缓存实例不被改写、dest 落盘内存值（`04_resources_scripts.json`）。
- reload_resource 重载缓存：replaced 后读回磁盘值；缺失文件报错（`04_resources_scripts.json`）。
- duplicate_resource 带 name 注册后跨调用引用与改写；copy_resource_file 复制（存在性/类型校验）+ 缺失源与同路径报错；get_resource_extensions / has_resource 只读查询（`04_resources_scripts.json`）。
- rename 事务化：目录级 rename 被拒绝并指引 move_resource_file；单文件 rename 返回 updated_files/uid_preserved/stale_references 结构化影响报告；find_in_files 证明旧路径引用消失、新路径引用存在；get_resource_references 前向反查按新路径命中、按旧路径为空（`05_rename_references.json`）。
- move_resource_file 引擎感知事务：引用方场景处于打开状态时磁盘引用改写、引用方场景自动重载、`.uid` sidecar 随迁保持 uid 不变、EFS 依赖图即时刷新（has_resource_dependency 反查命中目的地）；目录整体 move 断言 moved/filesystem_scanned 并以 find_in_files 与 get_resource_references 双向证明磁盘引用重写（`06_move_references.json`）。
- **回归点（ResourceUID add_id/set_id 写反事故）**：保存资源与 set_resource_uid 显式 uid（表内新 id）后 `ResourceUID.get_id_path` 回验注册成立；同一 uid 再指向第二文件走 set_id 更新分支改写路径；省略 uid 时走 add_id 创建分支（修复前两处写反，每次调用多一条引擎错误）（`22_uid_guard.json`）。
- **回归点（B3，根路径父目录切割事故）**：theme 五工具共用 persist_theme 与 create_script 曾把 `res://x.tres` 切出 `res:/` 导致建目录失败——根路径 create_theme_resource/set_theme_font_size 断言 saved/verified、get_theme_info/has_resource 读回、子目录自动建目录、根路径 create_script verified/readback/cache_refreshed（`31_theme_root.json`）。
- e2e 抽查建议：建资源→落盘引用→rename→move 一个事务序列，核对 uid 保持与磁盘引用双向改写；在 `res://` 根与子目录各落盘一次覆盖 B3。

## 域 8：脚本与 GDScript 执行

- execute_script 四行为：单表达式自动 return、多行显式 return、语法错误、缺参报错；`source_code` 别名生效、`expression`+`source_code` 双键冲突报错、双键全缺失报错（`04_resources_scripts.json`）。
- write_file 脚本诊断：坏 .gd 返回 `ok=false + diagnostic_scope=compile + parse_error.line`、好 .gd 返回 `compilable=true`、.gdshader 保持 `diagnostic_scope=load + compilable=unknown`、.cs 诚实标注 `resource_load + assembly_checked/is_dotnet_editor/hint`（`04_resources_scripts.json`）。
- **回归点（脚本缓存旧实例事故，CACHE_MODE_REUSE）**：create_script 写盘后必须让编辑器后续取用看到磁盘版本——断言 `cache_refreshed=true`（修复前无此字段）、attach_script_to_node 挂载后经 execute_script 读源码含新 marker（修复前挂上的仍是旧实例）、get_script_property `{fresh:true}` 读回新默认值、property_get/set 读写回（`18_script_freshness.json`）。
- CJK 落盘往返：含中文注释脚本 create_script 断言 verified/readback/cache_refreshed（godot-cpp `String(const char*)` latin1 构造曾致 3 字节 CJK 变 6 字节乱码）；read_file 全量内容逐字节比对；find_in_files 中文 query 命中（`19_cjk_roundtrip.json`；控制台输出侧的 GBK/ACP 回环由 L1 editor_output_decode_test 钉住）。
- 根路径 create_script（B3 同款切割修复）：见域 7 的 `31_theme_root.json` 条目。
- **code_execute 超时与预算（39_exec_budget.json）**：死循环脚本 + `timeout_ms=300` 返回 `code=exec_timeout` 结构化错误、不挂死编辑器主线程（协作式截止检查点注入）；正常脚本不被超时机制误伤；越界 `timeout_ms` 被参数校验拒绝不进入脚本执行。参数上限与宿主派发预算同源（静态上限 27000 ms 与长阻塞预算取 min，见 [tests.md](tests.md) 数值总表行）。
- e2e 抽查建议：demo 中真实跑一段含中文注释与 @export 的脚本（写盘→挂载→读属性链路），再故意跑一个死循环验证 exec_timeout 返回而非挂死。

## 域 9：瓦片地图

- fill_tilemap_rect 矩形铺砖：from/to 角点（含逆序归一）成功铺砖、erase 反序擦除、set_count 统计、单条 undo 动作；超 100000 格上限报错；缺 from/缺 source_id/缺 node_path 报错（`14_tilemap_rect.json`；归一与上限纯逻辑由 L1 tilemap_ops_test 钉住）。
- 夹具构建链：create_tilemap_tileset + add_tilemap_atlas_source + save_resource 落盘后挂到 TileMapLayer（`14_tilemap_rect.json`）。
- e2e 抽查建议：demo 含瓦片地图时核对一次矩形铺砖/擦除与 undo 单条性；无瓦片场景可跳过本域。

## 域 10：游戏运行时通道

- **前置：本域全部行为点需真实游戏进程**（编辑器 play 拉起、游戏侧加载 godot-autopilot 扩展）；无游戏时的进程内行为（参数校验、通道短路错误）已由集成层钉住，见 [tests.md](tests.md)「集成层」。
- start_game_job / get_game_job 参数校验：op 白名单（仅 eval）、缺 params、timeout 上限（25000，config 常量）、未知 job 的四段式错误、cancel 类型错误（`16_game_jobs.json`，集成层 `game_jobs_validation_test.cpp` 钉住）；**真实 job 生命周期**（start→pending→done 取回、expired 过期清理、cancel 取消）需要真实游戏进程，历史声明由 e2e 覆盖（`16_game_jobs.json` description 口径）。
- click_game_ui_element 坐标换算端到端：`window_position` 由游戏进程内 `run_ui_click` 计算（viewport→screen transform 复合 canvas transform），必须在真实游戏里点固定 rect 控件观测（历史补测骨架：320x180 视口 → 1280x720 窗口 4x 下 `window_position={640,360}`）；换算正确性由 L1 game_ui_coords_test 与 coords_inverse_test 覆盖纯函数层（`23_click_ui_coords.json` 覆盖力声明）。
- 键名入参在游戏进程内的判决：queue_game_input 的键名解析发生在游戏进程内，error+hint 两键可区分「被接受」与「被拒」（`24_keycode_alias.json` 游戏侧声明）。
- **回归点（B1 第三层）**：8 个 SceneTree 工具（set_scene_tree_pause / is_scene_tree_paused / reload_scene_tree_current_scene / set_scene_tree_debug_collisions_hint / call_scene_tree_group / notify_scene_tree_group / get_scene_tree_nodes_in_group / create_scene_tree_timer）统一经运行时通道转发到游戏进程，**编辑器自身场景树永不被触碰**——真实游戏中验证暂停态/计时器/组调用/重载落点确在游戏进程（历史代码缺陷曾操作编辑器树致插件停摆）（`35_scene_tree_gate.json` 回归守卫声明）。
- **回归点（B7，就绪门不可达）**：无游戏进程时默认 `wait_ready=true` 的游戏工具立即返回 `game not running: ...` 精确文案（修复前无 peer 的常驻会话也被计数致该路径几乎不可达）；`wait_ready=false` 走同一立即失败路径；就绪窗口（peer 已连接但 `gda:ready` 未到）的排队与重放需真实游戏进程复现（`37_game_ready_gate.json`；决策表由 L1 ready_policy_test 覆盖）。
- **回归点（class-body parse error 残留）**：execute_game_script 对裸语句（含 for/if/return）先包裹再编译（顶层 `wrapped=true`），成功调用不留 class-body parse error——同一游戏进程内 get_debugger_errors 断言 `total_errors=0` 起步（`45_runtime_observability.json`；包裹与信封抬升纯逻辑由 L1 gdscript_wrap_test / game_response_envelope_test 覆盖）。
- get_debugger_errors 错误增量游标：受控 push_error 后 `total_errors`/`total_groups`/`next_seq` 与 `since_seq` 排他过滤逐项断言（`45_runtime_observability.json`；游标纯逻辑由 L1 error_cursor_test 覆盖）。**环境敏感注记**：Godot 4.7.2 编辑器启动期会报一条引擎 ERROR（root certificate store，Windows 平台）计入编辑器错误缓冲，使 `total_errors` 的绝对计数断言差 1——绝对计数断言在 e2e 中改用增量（since_seq）口径。
- get_game_status 节点观测排除保留子树：经 execute_game_script 创建 `__gda_runtime` 命名的运行时子树后，`node_count`/`instrumentation_node_count` 不计该子树——插件自建节点不污染游戏侧观测计数（`45_runtime_observability.json` 的 instrumentation_node_count 阶段）。
- UI 命中校验（`46_ui_verification.json`）：get_game_ui_elements 暴露 `mouse_filter`/`z_index`；validate_game_ui_layout 把 mouse_filter=IGNORE 排除出 occluder、按 z_index 排序、返回 `possibly_occluded_count` 与 issues；click_game_ui_element 注入后读取真实 hover 断言 `hit_path` 精确指向遮挡容器且 `hit_mismatch=true`。
- get_game_ui_elements 的列表过滤（`46_ui_verification.json` 的 ui_filtering 阶段）：`filter_text` 文本过滤、`types` 类型过滤（button/label 各自命中）、`visible_only` 跳过隐藏控件（HiddenLabel 夹具）、过滤后 `truncated` 断言；capture 的 save/save_dir 越权拒绝在游戏 target 下同款成立（该用例的 save_validation 阶段）。
- e2e 抽查建议：play 拉起真实游戏后跑"execute_game_script 包裹 + 受控错误游标 + SceneTree 工具落点 + UI 遮挡命中"一条链，即覆盖本域全部历史回归点；游戏内断言一律用增量游标而非绝对计数。

## 域 11：C# 与 .NET

- **前置：本域真调行为点需 .NET 编辑器**（mono/dotnet）；判定权威信号是引擎注册了 `CSharpScript` 类（4.4+ 认 `dotnet` 特性名、更早认 `mono`、未知取二者或；Godot 4.x 未注册特性标签），由 L1 dotnet_env_test 覆盖纯函数层（`38_csharp_bridge.json` / `44_csharp_tooling.json` 降级依据段）。
- csharp_* 六域工具编辑器侧参数校验（集成层无此套件，历史 JSON 声明 30 步）：定位器互斥、必填成员名、runtime 取值、game-only 选项隔离、args 形态（非数组/超上限）、token 类型与正数（`38_csharp_bridge.json`）。
- locator 缺失覆盖：memory 不存在名返回四段式 memory-not-found 定位错误（定位先于桥单例获取，mono/非 mono 双稳）（`38_csharp_bridge.json`）。
- 反射真调口径（.NET 下验收）：私有成员门（`allow_non_public` 默认拒绝读/写 `_secret`、显式开启后取值）、固定元数方法调用、params 打包、泛型风格名 no-overload 全串、subscribe/unsubscribe token 链（`38_csharp_bridge.json` 真调骨架段）。
- list_members 过滤分页（纯编辑器侧、不依赖桥）：默认分页、`name_contains`、`limit/offset` 切片与越界、`kind` 过滤、truncated 语义；counts 计数来自引擎 ClassDB（`38_csharp_bridge.json` T-24/T-25 增补）。
- **回归点（res:// 交给 dotnet 构建不发生）**：`build_csharp_assembly` 原把 `res://` 工程文件路径原样交给 dotnet（MSB1009、构建不发生），修复为经 `ProjectSettings.globalize_path` 换算系统绝对路径；断言 result.project_file/command 契约与 `probe_script` 校验（非法 probe_script 在参数校验阶段拒绝）、`next_action`/`restart_required`/`editor_assembly_probe` 可读（`38_csharp_bridge.json`、`44_csharp_tooling.json`，changelog/2026-10-21-log.md）。
- 缺桥错误载荷与可执行片段：runtime=game 缺桥时返回含 `suggested_action` 与 `execute_game_script.source_code` 的可执行片段（load GdaCSharpRuntime.cs → new → add_child），用 execute_game_script 执行同源片段断言 bridge_created/bridge_ready（`38_csharp_bridge.json` game_bridge_missing 段）。
- **回归点（诊断假阳性分野）**：create_csharp_script 的 `diagnostics.ok`（ResourceLoader 可加载）与 `diagnostics.instantiable`（Script::can_instantiate）两字段分野，ok=true 而 instantiable=false 正是原假阳性现场（`44_csharp_tooling.json`）。
- **回归点（ACP 回环乱码）**：build_csharp_assembly 的 console_output 经 UTF-8 严格校验 + 系统代码页回退 + ACP 回环修复（还原 Godot OS_Windows::execute 以 CP_ACP 预解码 dotnet UTF-8 输出造成的「姝ｅ湪」类乱码），`output_digest` 不含已知乱码前缀作回归哨兵（`44_csharp_tooling.json`；解码纯逻辑由 L1 editor_output_decode_test 覆盖）。
- 运行期新建 C# 全局类在当次会话内不可实例化（.NET 程序集须编辑器启动时加载，GDExtension 无热重载 API）——属上游装载时序边界而非工具缺陷，历史上以 soft_error 前缀区分软失败；e2e 遇到同款报错按环境边界登记、不判工具回归（`38_csharp_bridge.json` 降级依据段，changelog/2026-10-21-log.md）。
- e2e 抽查建议：仅在 .NET 编辑器环境执行；核对 diagnostics 两字段分野与构建 output_digest 无乱码哨兵即可，反射真调按 demo 需要抽一两个成员访问。

## 域 12：动态用户工具

- 注册→调用→注销闭环：code_execute 内 `AutopilotTools.register_tool` 注册用户工具 → MCP call_tool 调用回显 → search_tools 命中且 get_tool_detail 标记 `dynamic=true` → code_execute 内 `api.call_tool` 直连（code-mode 桥）→ `get_tool` 取 handle 后 `unregister_tool` → 再调报 `domain tool ... not found`（`26_user_tools_code_mode.json`）。
- 目录扫描 rescan：code_execute 写 probe 工具文件（`register_autopilot_tools(api)` 实例方法约定）→ `api.rescan` 注册非空 → call_tool 回显 → 二次 rescan 幂等（registered 空、failed 存在）（`27_user_tools_rescan.json`）。
- **前置：code_execute + user_tools 能力**；user_tools 开关写配置自包含（env 优先于配置，env 不含 user_tools 时 set_enabled 无效）。
- e2e 抽查建议：demo 需要临时专用工具时走一次注册/调用/注销闭环，核对 dynamic 标记与注销后不可再调。

## 域 13：可观测性与主线程韧性

- system_status：服务器 running、`tool_count`/`queue_depth` 运行指标、`main_thread` 调度观测面五字段（last_drain_age_ms/queue_depth/queue_cancelled/queue_rejected_full/oldest_pending_ms）非空（`30_observability.json`、`36_stall_immunity.json`、`41_thread_guard.json`）。
- 存储布局：trace 与日志落 `工程级目录 <集中根>/projects/<项目名>-<路径哈希前 8 位>/{traces,logs}`（脚本经插件注入的存储路径环境变量定位后断言 layout_ok）（`29_trace_persistence.json`、`30_observability.json`、`41_thread_guard.json`；布局纯逻辑由 L1 storage_layout_test 覆盖）。
- trace jsonl 完整性：最新会话文件含 `session_start` 与 `server_ready` 启动标记（启动期入队、首帧 flush）；**每一非空行都是可解析 JSON**（主线程 flush 通道统一落盘后并发追加不再撕裂行）；含本轮 call_tool 事件行；全部 trace 文件不含 PNG base64 前缀（图片只记 `image_ref`/`image_hash`，脱敏默认开启时 store_trace_image 返回空）（`29_trace_persistence.json`、`41_thread_guard.json`）。
- trace schema v2：`kind=lifecycle`（启动期）/`kind=snapshot`（environment_snapshot）/`kind=protocol_request` 与 `request_id`/`correlation_id`/`thread_id`/`monotonic_ns` 可观测键（`30_observability.json`）。
- 人类日志：gda-*.log 含启动日志行（"Godot Self-Driving plugin starting"）与 `trace=` 关联 detail 行（`29_trace_persistence.json`、`30_observability.json`、`41_thread_guard.json`）。
- **回归点（B1 第二层，通道停摆免疫）**：编辑器 SceneTree 被暂停（`get_tree().paused=true`，模拟暂停/模态/停摆态）期间，ping（kHealthProbe 走 5000ms 短预算路径）与 system_status 仍可应答（插件 `_enter_tree` 设 `PROCESS_MODE_ALWAYS` 继续 drain，changelog/2026-09-29-log.md）；健康探针停摆时于预算内返回结构化 main_thread_timeout 而非挂死；恢复后 ping 正常（`36_stall_immunity.json`）。
- **已知边界（不判缺陷）**：编辑器进程的引擎错误不可经 MCP 观测（`user://logs/godot.log` 只由游戏进程写；`get_debugger_log` 读编辑器缓冲但无过滤计数能力）——22 号曾用 get_game_log_entries 做引擎错误守卫，因 headless 床无游戏进程属真空覆盖已移除（changelog/2026-09-26-log.md，旧 tests.md「已知遗留」）。
- e2e 抽查建议：每轮 e2e 收尾时扫一次 trace jsonl（行可解析、无 PNG base64）与日志启动行，属于零成本的持续回归哨兵。

## 历史事故回归点索引

上表各域内已就地标注「回归点」；此处按事故汇总（原始记载均存于 changelog 或 git 历史）：

| 事故 | 域 | 现行回归验收点 | 来源 |
|---|---|---|---|
| Example 污染（`set_editor_main_scene` 写 `main_scene="test"`；`save_editor_scene` 空参生成 NewNode.tscn） | 1 | 排除声明纪律：写配置/文件/弹窗/窗口的工具必须声明 side_effect（+kMutating） | changelog/2026-09-18-log.md；提交 `ca1461c` 考古 |
| 启发式冒烟写 [input] 段（add_input_map_action 声明 None 未排除） | 1、5 | 同上；InputMap 持久化工具声明核对 | changelog/2026-09-29-log.md |
| B1 编辑器树被游戏工具触碰致插件停摆 | 10、13 | SceneTree 工具只落游戏进程；暂停态下 ping/system_status 仍应答 | changelog/2026-09-29-log.md |
| B2 dry_run 假校验 | 3 | dry_run 与真实写入同路径同结论 | changelog（B2 批次）；`32_plane_inline.json` |
| B3 根路径父目录切割（`res:/`） | 7、8 | theme/script 根路径落盘 saved/verified | changelog（B3 批次）；`31_theme_root.json` |
| B4 含子场景实例保存误判 mismatch | 4 | 继承节点分离计数与 inherited_paths | changelog（B4 批次）；`33_verified_inherit.json` |
| B5 引擎原生斜杠属性被拒 | 3 | 斜杠属性直写 + 族展开 + 深路径 | changelog（B5 批次）；`34_slash_property.json` |
| B7 就绪门不可达（无 peer 常驻会话被计数） | 10 | 无游戏时 `game not running:` 立即失败 | changelog（B7 批次）；`37_game_ready_gate.json` |
| ResourceUID add_id/set_id 写反 | 7 | 保存/显式 uid 后 get_id_path 回验 | `22_uid_guard.json` |
| 脚本缓存旧实例（CACHE_MODE_REUSE） | 8 | cache_refreshed 字段 + attach 取磁盘版链路 | `18_script_freshness.json` |
| CJK latin1 构造乱码 | 8 | create_script verified/readback + read_file 逐字节 | `19_cjk_roundtrip.json` |
| 属性两轮应用顺序（animation 先于 sprite_frames 清空） | 3 | applied_properties 精确序 + 读回 | `28_sprite_frames_animation.json` |
| open_editor_scene root 不变误报 | 4 | 幂等 already_open + reload_if_open 前后核验 | `25_open_scene_idempotent.json` |
| class-body parse error 残留 | 10 | execute_game_script 包裹后错误缓冲零残留 | `45_runtime_observability.json` |
| res:// 交给 dotnet（MSB1009 构建不发生） | 11 | build_csharp_assembly 路径换算与探测字段 | changelog/2026-10-21-log.md |
| ACP 回环乱码（姝ｅ湪） | 11 | output_digest 乱码哨兵 | changelog/2026-10-21-log.md；`44_csharp_tooling.json` |
| warnings 历史缺口（4 工具空参不报错） | 1 | 空参必报缺参（基线 0 warnings） | 旧 tests.md「warnings 语义」（`0dad560~2` git 历史） |
| editor_layout.cfg open_scenes 残留漂移 | 4 | e2e 批量异常先查 `.godot/editor` 生成物 | changelog/2026-09-26-log.md、2026-09-29-log.md |
| 授权拒绝路径无自动断言缺口 | 2 | 47 号默认拒绝态结构化断言 | `47_authorization_gate.json` |
| Godot 4.7.2 启动期 root certificate store ERROR（环境敏感） | 10 | 错误计数用增量游标而非绝对计数 | 旧 tests.md isolated 名单节（git 历史） |

> 附注：mcp-cpp-sdk 客户端响应等待缺陷属已删除测试客户端（`tests/runner`）的缺陷，生产服务端不受影响，不构成引擎行为验收点，此处仅存目（旧 tests.md「已知遗留」）。

## 与 gda-e2e-testing 的衔接

- 验收执行走 [gda-e2e-testing 技能](../../.agents/skills/gda-e2e-testing/SKILL.md)：以 `examples/<名称>` 为场地、按其任务书与验收清单推进，本页的域条目与回归点作为**验收时的行为对照面**（发现偏离即记入该技能产出的问题记录）。
- 本页不改变该技能流程；分析闭环由 [gda-e2e-analysis 技能](../../.agents/skills/gda-e2e-analysis/SKILL.md) 承接。测试计数口径见 [tests.md](tests.md)「数值核算总表」。

## A 类迁移对照表

> 48 份 L2 用例的「集成层迁移 + 划回 e2e」合并定稿。等价口径：错误串逐字一致（C-06，含 em dash 与引号转义）；结构键与值一致；`not_empty` 声明以非空断言对应；env 注入等价复刻旧 runner `requires` 语义。集成层规模 **109 项 / 12 文件**（引用 [tests.md](tests.md)「数值核算总表」集成行，不自造口径）。原始四份来源：`.handoff/evidence/T-02/mapping.md`（00/10/47 试点）、`T-04/mapping-12-16-17-39.md`、`T-05/mapping-23-24-35-37-40.md`、`T-06/mapping-toolface.md`（03 工具面契约）；R-01 台账（引擎触达剔除记录）在各来源文件内。

### 已迁用例对照

| 源 JSON 步骤 | 集成层 gtest 断言 | 等价结论 | 划回 e2e 项 |
|---|---|---|---|
| `00_meta`：ping_ok / search_keyword / search_category / search_tags / list_categories / detail_existing（6 步） | `meta_tools_test.cpp` MetaToolsSuite 前 6 TEST | 结构等价（非空断言 + tool.* 形状 + schema.type=object） | — |
| `00_meta`：detail_missing / detail_missing_param / call_unknown_tool / call_missing_name / batch_sequential / batch_unknown_tool（6 步） | MetaToolsSuite 后 6 TEST | 错误串与计数逐字等价 | — |
| `00_meta`：code_return_value / code_syntax_error / code_missing_source | 不迁 | — | 真实 GDScript 编译执行（T-02 R-01-01/02）；缺参步因 handler 首段栈上 `godot::Variant` 构造在进程内 SEH（R-01-03），错误串等价性由源码背书 |
| `10_editor_input`：click_input_mouse_missing_position / scroll_input_mouse_invalid_direction / drag_input_mouse_missing_to / type_input_text_missing_text / click_editor_element_missing_path / run_editor_shortcut_unknown_key | `editor_input_validation_test.cpp` EditorInputSuite 6 TEST | 逐字等价（全部注入前返回，无副作用） | — |
| `47_authorization_gate`：create_scene_node_denied | `authorization_gate_test.cpp` AuthorizationGate.CreateSceneNodeDeniedByDefault | 逐字等价（拒绝全串 + code + authorization_required=write） | — |
| `12_capture_params`：after_frames_negative / after_frames_wrong_type / after_frames_editor_target / when_editor_target / scale_below_min / scale_above_max / game_capture_after_frames_no_game / editor_save_wrong_type / editor_save_dir_wrong_type / editor_save_dir_escape / editor_save_dir_user_scheme / game_save_wrong_type / game_save_dir_escape（13 步） | `capture_params_validation_test.cpp` CaptureParamsSuite 13 TEST | 前 12 步逐字等价；game_capture_after_frames_no_game 等价（JSON 仅声明 error/hint 非空） | editor_save_dir_outside_project（T-04 R-04-01：绝对路径分支触 `ProjectSettings::get_singleton`，进程内 SEH；错误串由源码背书） |
| `16_game_jobs`：start_missing_op / start_unsupported_op / start_missing_params / start_timeout_out_of_range / start_valid_args_no_game / get_missing_job_id / get_unknown_job_id / get_cancel_wrong_type / get_timeout_negative（9 步） | `game_jobs_validation_test.cpp` GameJobsSuite 9 TEST | 前 4 步与后 4 步逐字等价（含 em dash 四段式）；start_valid_args_no_game 等价（非空声明） | 真实 job 生命周期（start→pending→done / expired / cancel）本就声明不做断言（JSON 口径），划回 e2e（域 10） |
| `17_vision_assist`：annotate_nodes_not_array / _empty / _empty_string / _max_not_int / _max_out_of_range / _exceeds_self_max / diff_image_not_bool / diff_image_game_target / game_annotate_nodes_empty / _empty_string / _max_zero / review_unknown_param / review_both_skipped / review_annotate_nodes_empty / _empty_string / _max_zero / review_node_viewport_bad / review_max_dimension_bad / review_scale_bad / review_region_bad / review_include_editor_not_bool（21 步） | `vision_assist_validation_test.cpp` VisionAssistSuite 21 TEST | 逐字等价 | diff_image_requires_diff（T-04 R-04-02：`EditorInterface::get_singleton` 先于该校验，进程内 SEH；错误串由源码背书） |
| `23_click_ui_coords`：missing_path / path_wrong_type / button_index_out_of_range / double_click_wrong_type / max_elements_non_positive / timeout_out_of_range / unknown_parameter / coord_space_rejected_on_click / window_rect_rejected_on_click / queue_coord_space_illegal / sequence_coord_space_illegal（11 步） | `click_ui_coords_validation_test.cpp` ClickUiCoordsSuite 11 TEST | 逐字等价（coord_space 校验先于通道） | valid_args_without_running_game / queue_viewport_space_without_running_game / ui_elements_without_running_game（T-05 R-01-01：`game not running:` 精确文案依赖编辑器态 DebugCapturePlugin，进程内恒为 `debugger capture plugin not initialized`，C-06 下不得以替代文案断言） |
| `24_keycode_alias`：queue_keycode_prefixed_letter / queue_keycode_bare_letter（2 步） | `queue_game_input_keycode_alias_test.cpp` KeycodeAliasSuite 2 TEST | 等价（JSON 声明 error/hint 非空，进程内通道错误满足声明；「无 hint 即被拒」的区分力在游戏进程内，见 R-01-02 注） | 编辑器侧 9 步（add_probe_action + keycode_bare_letter…keycode_unknown_name，T-05 R-01-02：`InputMap::get_singleton` 在键名归一之前，进程内 SEH；键名判决由 L1 keycode_alias_test 全量覆盖背书）+ before/after 探针清理 2 步（域 5 的 e2e 动作） |
| `35_scene_tree_gate`：gate_set_pause_no_game / gate_is_paused_no_game / gate_reload_scene_no_game / gate_debug_collisions_no_game / gate_call_group_no_game / gate_notify_group_no_game / gate_nodes_in_group_no_game / gate_create_timer_no_game（8 步 × 授权/通道两态） | `scene_tree_gate_test.cpp` SceneTreeGateDenied + SceneTreeGateAllowed 16 TEST | 等价（JSON 声明 error 非空，两态均成立；辅助断言以 `authorization required` 有无区分两态） | scene_tree_gate_before_scene / gate_editor_tree_not_paused（B1 回归守卫须真实引擎）/ gate_save_scene / gate_close_scene / gate_trash_dir（T-05 R-01-03，域 4 与域 10 的 e2e 动作） |
| `37_game_ready_gate`：eval_wait_default_no_game / capture_wait_default_no_game / eval_wait_false_no_game / capture_wait_false_no_game | 不迁（零可迁步，未建测试文件，T-05 R-01-04） | — | 全部 4 步（`game not running:` 精确全串同 R-01-01 根因）；决策表由 L1 ready_policy_test 背书 |
| `40_contract_assert`：code_execute_side_effect | `tool_detail_contract_test.cpp` ContractAssertSuite.CallToolGetToolDetailCodeExecuteContract | 逐字等价（registry 契约：`side_effect_name(CodeExecute)="code_execute"` 三方一致） | code_execute_result_key（T-05 R-01-05：真实脚本执行，同 R-01-03 机理） |
| `39_exec_budget`：exec_infinite_loop_times_out / exec_fast_script_returns_result / exec_timeout_ms_out_of_range_rejected | 不迁（零可迁步，未建测试文件，T-04 R-04-03） | — | 全部 3 步（真实脚本执行 + handler 首段栈上 Variant 边界）；行为点落域 8 |
| `03_tools_contract`（工具面契约，D-02=A′ 动态枚举承接） | `toolface_contract_test.cpp` 16 TEST：SearchToolsEmptyQueryEnumeratesAllTools / MetaToolsDistinguishableByCategoryField（探测断言）/ DetailContractForEveryEnumeratedTool / DetailContractUnknownNameReportsToolNotFound（语义等价，精确全串已由 `meta_tools_test.cpp` 逐字覆盖）/ ExclusionPartitionCoversAllEnumeratedTools / EmptyArgCallRejects 11 组（9 档位授权拒绝三形态 + Mutating + Dynamic 缺参语义） | 语义等价：排除判定（side_effect 非 none / mutating / dynamic）与授权拒绝形态由运行时枚举动态复算，无硬编码工具名单；计数（枚举/排除/候选/步数/warnings）为 GTEST_SUCCEED 观察值，不作断言钉 | 全量遍历的真实引擎往返（empty_args + heuristic_smoke 对真实编辑器）与 reload_resource 缺参精确串（候选组空参实调因进程内 SEH 整组剔除，执行期处置：17→44→36→16 TEST 演进见 T-06 mapping 执行期增补），随 e2e 工具调用自然发生 |

### 未迁用例划回 e2e 汇总

未迁 35 份（01/02/04/05/06/07/08/09/11/13/14/15/18/19/20/22/25/26/27/28/29/30/31/32/33/34/36/38/41/42/43/44/45/46/48）+ `03` 的遍历往返部分，按域归并；逐文件语义已体现于上方各域条目：

| 域 | 划回用例 | e2e 去向 |
|---|---|---|
| 3. 场景节点与属性 | 01、02、08、13、20、28、32、34、42（写入链路） | 真实编辑器中的建节点/属性读写回读/内联子资源落盘；e2e 日常建场景与改属性动作即验收面，重点对照 B2/B5/P2-2 回归点 |
| 4. 编辑器会话与场景文件 | 07、25、33、43、15 | 多场景轮换（建→脏→切→reload→落盘→核验）与写响应 scene_path 回显观察 |
| 5. 编辑器 UI 与输入 | 09、11、10（正向路径）、24（编辑器侧 keycode 真调） | headless/GUI 编辑器 UI 枚举、场景树选中联动与真实 UI 点击（历史未自动化缺口） |
| 6. 截图与视觉辅助 | 12（合法落盘路径）、17（正向）、48 | GUI 游戏帧相位抓拍、连续 save 落盘清单核对、越权路径拒绝 |
| 7. 资源与引用事务 | 04（资源面）、05、06、22、31（theme） | 建资源→引用→rename→move 事务序列、UID 流转、根/子目录落盘 |
| 8. 脚本与 GDScript 执行 | 04（execute_script 与诊断）、18、19、31、39 | 真实脚本编译执行、缓存刷新链路、CJK 落盘、exec_timeout 超时行为 |
| 9. 瓦片地图 | 14 | demo 含瓦片时的铺砖/擦除/undo 与夹具构建链 |
| 10. 游戏运行时通道 | 16（生命周期）、23（坐标换算）、24（游戏侧）、35（游戏内行为）、37（就绪窗口）、45、46 | 真实游戏进程（play + 游戏侧扩展）下的 job 生命周期、坐标换算端到端、SceneTree 落点、错误增量游标、UI 遮挡命中 |
| 11. C# 与 .NET | 38、44 | .NET 编辑器下反射真调、程序集构建、诊断分野、编码哨兵（见域 11 前置） |
| 12. 动态用户工具 | 26、27 | 用户脚本工具注册/调用/注销与 rescan 生命周期 |
| 13. 可观测性与主线程韧性 | 29、30、41、36 | 每轮 e2e 收尾扫 trace jsonl 行完整性与日志启动行；模拟暂停态验证通道应答（可选） |
| 1. 元工具与工具面契约 | 03（遍历往返与 reload 缺参契约步）、42（写入链路正向） | 遍历语义已由动态契约承接后，真实往返随 e2e 工具调用自然发生；授权拒绝由集成层 `authorization_gate_test.cpp` 钉住 |

## 相关页面

- [测试体系（tests.md）](tests.md)——集成层 109 项的套件覆盖清单、回归属性清单与数值核算总表
- [工具注册表（modules/tools_registry.md）](modules/tools_registry.md)——授权三档、排除划分与遍历判定
- [领域工具 A 组（modules/tools_ops_a.md）](modules/tools_ops_a.md) / [B 组（modules/tools_ops_b.md）](modules/tools_ops_b.md)——各域工具实现细节
- [gda-e2e-testing 技能](../../.agents/skills/gda-e2e-testing/SKILL.md)——验收执行载体
