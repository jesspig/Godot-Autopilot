import fs from "node:fs";
import path from "node:path";

import { PROJECT_ROOT } from "../lib/config";

export type ToolAuditOptions = {
  json: boolean;
  baseline: boolean;
  target: boolean;
  report: boolean;
  out: string | null;
};

type ToolRecord = {
  name: string;
  category: string;
  domain: string;
  side_effect: string;
  flags: string[];
  param_count: number | null;
  has_action_schema?: boolean;
  source: string;
  line: number;
};

type PruneRow = {
  old_tool: string;
  new_tool: string;
  action: string;
  arg_adapt: string;
  result_projection: string;
};

type DomainTarget = {
  target: number;
  plan_target: number;
  merge_groups: string[];
  note?: string;
};

const DOMAINS_DIR = path.join(PROJECT_ROOT, "src", "tools", "domains");
const REGISTER_ALL = path.join(PROJECT_ROOT, "src", "tools", "base", "register_all.cpp");
const DATA_DIR = path.join(PROJECT_ROOT, "tests", "data");
const BASELINE_PATH = path.join(DATA_DIR, "tool_catalog_baseline.json");
const TARGET_PATH = path.join(DATA_DIR, "tool_surface_target.json");
const MATRIX_PATH = path.join(DATA_DIR, "capability_matrix.json");

const DOMAIN_LABELS: Record<string, string> = {
  analyze: "Analysis",
  animation: "Animation",
  audio: "Audio",
  capture: "Capture",
  config: "Config",
  csharp_bridge: "C#",
  debug: "Debug",
  debugger: "Debugger",
  image: "Image",
  mesh: "Mesh",
  display: "Display",
  doc: "Docs",
  editor: "Editor",
  game: "Game",
  gridmap: "GridMap",
  group: "Group",
  input: "Input",
  input_map: "InputMap",
  log: "Log",
  monitor: "Monitor",
  nav: "Nav",
  os: "OS",
  physics: "Physics",
  property: "Properties",
  render: "Render",
  resource: "Resources",
  scene: "Scene",
  scene_tree: "SceneTree",
  script: "Scripts",
  spriteframes: "SpriteFrames",
  test: "Testing",
  text: "Text",
  theme: "Theme",
  tilemap: "TileMap",
  tileset: "Tileset",
  tween: "Tween",
};

const DOMAIN_TARGETS: Record<string, DomainTarget> = {
  Analysis: { target: 1, plan_target: 1, merge_groups: ["analyze_project"] },
  Animation: {
    target: 6,
    plan_target: 6,
    merge_groups: ["animation_clip", "animation_track", "animation_player", "animation_tree"],
  },
  Audio: {
    target: 13,
    plan_target: 13,
    merge_groups: ["audio_bus", "audio_player", "audio_device", "audio_capture", "audio_wav"],
  },
  Capture: { target: 2, plan_target: 2, merge_groups: ["capture_editor_viewport", "review_scene_visually"] },
  "C#": { target: 3, plan_target: 3, merge_groups: ["csharp_member", "csharp_event", "dotnet"] },
  Config: { target: 11, plan_target: 11, merge_groups: ["config_read", "config_write", "autoload"] },
  Image: {
    target: 4,
    plan_target: 4,
    merge_groups: ["image_edit", "image_pixel", "image_save", "texture_gen"],
  },
  Mesh: {
    target: 4,
    plan_target: 4,
    merge_groups: ["mesh_surface", "mesh_inspect", "multimesh", "multimesh_instance"],
  },
  Debug: {
    target: 9,
    plan_target: 10,
    merge_groups: [
      "debug_info",
      "debug_stack",
      "debug_log",
      "debug_visual",
      "debug_physics_fps",
      "debug_monitors",
    ],
    note: "D-08 记当前 16，源码实测为 15；D-07 删除 6 项后为 9，故有效目标取 9",
  },
  Debugger: {
    target: 4,
    plan_target: 4,
    merge_groups: ["debugger_stream", "debugger_plugin_log", "debugger_game_log", "debugger_session"],
  },
  Display: {
    target: 14,
    plan_target: 14,
    merge_groups: [
      "display_info",
      "display_clipboard",
      "display_dialog",
      "display_mouse",
      "display_tts",
      "display_window",
      "display_screen",
    ],
  },
  Docs: { target: 1, plan_target: 1, merge_groups: ["get_docs_class"] },
  Editor: {
    target: 24,
    plan_target: 24,
    merge_groups: [
      "editor_state",
      "editor_ui",
      "editor_files",
      "editor_scene_control",
      "editor_save",
      "editor_selection",
      "editor_undo",
      "editor_ui_action",
      "editor_config",
      "editor_inspect",
      "editor_csharp",
      "editor_script",
      "editor_breakpoints",
      "editor_session",
      "editor_info",
    ],
  },
  Game: {
    target: 13,
    plan_target: 13,
    merge_groups: [
      "game_state",
      "game_job",
      "game_ui",
      "game_evidence",
      "game_script",
      "game_input",
      "game_tween",
      "game_localization",
    ],
  },
  GridMap: {
    target: 4,
    plan_target: 4,
    merge_groups: ["create_gridmap", "gridmap_item", "set_gridmap_cells", "get_gridmap_used_cells"],
  },
  Group: { target: 1, plan_target: 1, merge_groups: ["node_group"] },
  Input: {
    target: 7,
    plan_target: 7,
    merge_groups: ["input_state", "input_action", "input_key", "input_mouse", "input_text", "input_gamepad"],
  },
  InputMap: {
    target: 5,
    plan_target: 5,
    merge_groups: ["get_input_map_actions", "set_input_map_action_bundle", "erase_input_map", "save_input_map"],
  },
  Log: { target: 1, plan_target: 1, merge_groups: ["get_game_log_entries"] },
  Monitor: { target: 1, plan_target: 1, merge_groups: ["game_monitor"] },
  Nav: { target: 6, plan_target: 6, merge_groups: ["create_navigation", "nav_query", "nav_control"] },
  OS: {
    target: 11,
    plan_target: 11,
    merge_groups: [
      "process",
      "env",
      "time",
      "system",
      "file",
      "file_manager",
      "alert",
      "path",
    ],
  },
  Physics: {
    target: 32,
    plan_target: 32,
    merge_groups: [
      "body",
      "area",
      "space",
      "joint",
      "shape",
      "query",
      "soft_body",
      "debug",
      "bundle",
    ],
  },
  Properties: {
    target: 6,
    plan_target: 6,
    merge_groups: ["property_get", "property_set", "set_node_transform", "property_get_list", "signal"],
  },
  Render: {
    target: 30,
    plan_target: 30,
    merge_groups: [
      "environment",
      "instance",
      "viewport",
      "camera",
      "canvas",
      "light",
      "particles",
      "resource",
      "inspect",
      "free",
    ],
  },
  Resources: {
    target: 20,
    plan_target: 20,
    merge_groups: [
      "load",
      "query",
      "create",
      "property",
      "file",
      "save",
      "uid",
      "import",
      "curve",
      "mesh",
    ],
  },
  Scene: {
    target: 9,
    plan_target: 9,
    merge_groups: [
      "get_scene_tree",
      "create_scene_node",
      "edit_scene_node",
      "delete_scene_node",
      "camera",
      "instantiate",
    ],
  },
  SceneTree: { target: 1, plan_target: 1, merge_groups: ["game_scene_tree"] },
  Scripts: {
    target: 6,
    plan_target: 6,
    merge_groups: ["query", "file", "attach", "execute", "call"],
  },
  SpriteFrames: { target: 2, plan_target: 2, merge_groups: ["update", "get"] },
  Testing: { target: 1, plan_target: 1, merge_groups: ["run_tests"] },
  Text: { target: 4, plan_target: 4, merge_groups: ["font", "shaped", "query"] },
  Theme: { target: 4, plan_target: 4, merge_groups: ["info", "update_theme", "update_layout"] },
  TileMap: { target: 3, plan_target: 3, merge_groups: ["create", "set_cells", "tileset"] },
  Tileset: { target: 1, plan_target: 1, merge_groups: ["update_tileset"] },
  Tween: { target: 2, plan_target: 2, merge_groups: ["game_tween", "editor_preview"] },
};

const NEW_TOOLS = [
  { name: "pack_scene", domain: "Scene", purpose: "PackedScene::pack 磁盘打包与 SceneState 读取" },
  { name: "animation_runtime", domain: "Animation", purpose: "AnimationPlayer 播放/seek/速度与轨道读取" },
  { name: "bake_navigation_mesh", domain: "Nav", purpose: "NavigationMeshGenerator 烘焙" },
  { name: "astar_path", domain: "Nav", purpose: "AStarGrid2D/AStar2D/AStar3D 路径查询" },
  { name: "image_ops", domain: "Image", purpose: "Image/Noise/Gradient 生成与保存" },
  { name: "mesh_ops", domain: "Mesh", purpose: "SurfaceTool/Mesh/MultiMesh 生成与碰撞" },
];

const HOT_NAMES = [
  "property_get",
  "property_set",
  "create_scene_node",
  "get_scene_tree",
  "capture_editor_viewport",
  "call_tool",
];

const META_TOOLS_WITH_PARAMS = new Set([
  "search_tools",
  "get_tool_detail",
  "call_tool",
  "batch_execute",
  "code_execute",
]);

const PREVIOUS_META_TOOLS = new Set(["ping", "list_categories"]);

const PRUNE_ROWS: PruneRow[] = [
  {
    old_tool: "get_audio_device_inputs",
    new_tool: "get_audio_input_devices",
    action: "",
    arg_adapt: "无",
    result_projection: "旧 result 取 devices",
  },
  {
    old_tool: "get_audio_bus_count",
    new_tool: "get_audio_bus_layout",
    action: "count",
    arg_adapt: "无",
    result_projection: "buses.size()",
  },
  {
    old_tool: "get_audio_bus_name",
    new_tool: "get_audio_bus_layout",
    action: "name",
    arg_adapt: "bus_index",
    result_projection: "buses[bus_index].name",
  },
  {
    old_tool: "get_debug_monitor",
    new_tool: "get_debug_monitors",
    action: "values",
    arg_adapt: "monitor -> ids=[monitor]",
    result_projection: "取首个 value",
  },
  {
    old_tool: "get_debug_monitor_catalog",
    new_tool: "get_debug_monitors",
    action: "catalog",
    arg_adapt: "catalog=true",
    result_projection: "取 catalog 数组",
  },
  {
    old_tool: "get_debug_object_count",
    new_tool: "get_debug_monitors",
    action: "values",
    arg_adapt: "ids=[OBJECT_COUNT]",
    result_projection: "取 OBJECT_COUNT 值",
  },
  {
    old_tool: "get_debug_memory_usage",
    new_tool: "get_debug_monitors",
    action: "values",
    arg_adapt: "ids=[MEMORY_STATIC]",
    result_projection: "取 MEMORY_STATIC 值",
  },
  {
    old_tool: "get_debug_node_count",
    new_tool: "get_debug_monitors",
    action: "values",
    arg_adapt: "ids=[OBJECT_NODE_COUNT]",
    result_projection: "取 OBJECT_NODE_COUNT 值",
  },
  {
    old_tool: "get_debug_custom_monitor_names",
    new_tool: "get_debug_monitors",
    action: "custom",
    arg_adapt: "include_custom=true",
    result_projection: "取 custom_names 数组",
  },
  {
    old_tool: "get_docs_method",
    new_tool: "get_docs_class",
    action: "member",
    arg_adapt: "section=methods, member=method, exact=true",
    result_projection: "取单条 method 字典",
  },
  {
    old_tool: "get_docs_property",
    new_tool: "get_docs_class",
    action: "member",
    arg_adapt: "section=properties, member=property, exact=true",
    result_projection: "取单条 property 字典",
  },
  {
    old_tool: "set_theme_color",
    new_tool: "set_theme_item_bundle",
    action: "set",
    arg_adapt: "{theme_type:{varname:color}} -> colors",
    result_projection: "旧 {result:\"ok\"}",
  },
  {
    old_tool: "set_theme_constant",
    new_tool: "set_theme_item_bundle",
    action: "set",
    arg_adapt: "{theme_type:{varname:constant}} -> constants",
    result_projection: "旧 {result:\"ok\"}",
  },
  {
    old_tool: "set_theme_font_size",
    new_tool: "set_theme_item_bundle",
    action: "set",
    arg_adapt: "{theme_type:{varname:size}} -> font_sizes",
    result_projection: "旧 {result:\"ok\"}",
  },
  {
    old_tool: "set_theme_stylebox_flat",
    new_tool: "set_theme_item_bundle",
    action: "set",
    arg_adapt: "{theme_type:{varname:stylebox}} -> styleboxes",
    result_projection: "旧 {result:\"ok\"}",
  },
  {
    old_tool: "set_control_anchor_preset",
    new_tool: "set_control_layout",
    action: "anchor",
    arg_adapt: "control_path -> path; preset/keep_offsets 透传",
    result_projection: "旧 {result:\"ok\"}",
  },
  {
    old_tool: "set_tilemap_cell",
    new_tool: "set_tilemap_cells",
    action: "set",
    arg_adapt: "单格包装为 cells:[{...}]",
    result_projection: "旧 {result:\"ok\"}",
  },
  {
    old_tool: "set_gridmap_cell",
    new_tool: "fill_gridmap_box",
    action: "fill",
    arg_adapt: "position -> from=to=position",
    result_projection: "旧 {result:\"ok\"}",
  },
  {
    old_tool: "add_input_map_action",
    new_tool: "set_input_map_action_bundle",
    action: "set",
    arg_adapt: "action/deadzone",
    result_projection: "旧 {result:\"ok\"}",
  },
  {
    old_tool: "add_input_map_action_event",
    new_tool: "set_input_map_action_bundle",
    action: "set",
    arg_adapt: "events:[event]",
    result_projection: "旧 {result:\"ok\"}",
  },
  {
    old_tool: "set_input_map_action_deadzone",
    new_tool: "set_input_map_action_bundle",
    action: "set",
    arg_adapt: "deadzone",
    result_projection: "旧 {result:\"ok\"}",
  },
  {
    old_tool: "has_input_map_action",
    new_tool: "get_input_map_actions",
    action: "has",
    arg_adapt: "action",
    result_projection: "旧布尔",
  },
  {
    old_tool: "has_resource_dependency",
    new_tool: "get_resource_dependencies",
    action: "has",
    arg_adapt: "dependency",
    result_projection: "旧布尔",
  },
  {
    old_tool: "move_resource_file",
    new_tool: "rename_resource_file",
    action: "move",
    arg_adapt: "path + new_directory",
    result_projection: "旧 move 返回",
  },
  {
    old_tool: "load_script",
    new_tool: "load_resource",
    action: "load",
    arg_adapt: "type_hint=Script; fresh -> cache_mode",
    result_projection: "旧 script 引用",
  },
  {
    old_tool: "set_properties_batch",
    new_tool: "property_set",
    action: "batch",
    arg_adapt: "properties + atomic",
    result_projection: "旧 batch 返回",
  },
  {
    old_tool: "get_scene_snapshot",
    new_tool: "get_scene_tree",
    action: "snapshot",
    arg_adapt: "verify/paths/observe",
    result_projection: "旧 snapshot 信封",
  },
  {
    old_tool: "get_scene_node_screen_rect",
    new_tool: "get_scene_tree",
    action: "screen_rect",
    arg_adapt: "paths",
    result_projection: "旧 nodes 段",
  },
  {
    old_tool: "verify_scene_saved",
    new_tool: "get_scene_tree",
    action: "verify",
    arg_adapt: "verify=true",
    result_projection: "旧 verify 对象",
  },
  {
    old_tool: "capture_game_viewport",
    new_tool: "capture_editor_viewport",
    action: "game",
    arg_adapt: "target=\"game\"",
    result_projection: "旧截图信封",
  },
  {
    old_tool: "ping",
    new_tool: "system_status",
    action: "",
    arg_adapt: "无",
    result_projection: "{result:\"pong\"}",
  },
  {
    old_tool: "list_categories",
    new_tool: "search_tools",
    action: "categories",
    arg_adapt: "categories=true, limit=0",
    result_projection: "旧 categories 数组",
  },
];

const MUTATION_PREFIXES = [
  "remove_",
  "delete_",
  "destroy_",
  "free_",
  "quit_",
  "restart_",
  "run_",
  "execute_",
  "launch_",
  "play_",
  "stop_",
  "press_",
  "click_",
  "type_",
  "inject_",
  "bake_",
  "import_",
  "reimport_",
  "export_",
  "move_",
  "rename_",
  "duplicate_",
  "instantiate_",
  "connect_",
  "disconnect_",
  "send_",
  "write_",
  "save_",
];

function relativeToRoot(file: string): string {
  return path.relative(PROJECT_ROOT, file).split(path.sep).join("/");
}

function findBlockEnd(text: string, open: number): number {
  if (text[open] !== "{") throw new Error(`expected '{' at offset ${open}`);
  let depth = 0;
  let state: "code" | "line" | "block" | "string" | "char" = "code";
  for (let index = open; index < text.length; index += 1) {
    const char = text[index];
    const next = text[index + 1];
    if (state === "line") {
      if (char === "\n") state = "code";
      continue;
    }
    if (state === "block") {
      if (char === "*" && next === "/") {
        state = "code";
        index += 1;
      }
      continue;
    }
    if (state === "string" || state === "char") {
      if (char === "\\") {
        index += 1;
        continue;
      }
      if ((state === "string" && char === "\"") || (state === "char" && char === "'")) {
        state = "code";
      }
      continue;
    }
    if (char === "/" && next === "/") {
      state = "line";
      index += 1;
      continue;
    }
    if (char === "/" && next === "*") {
      state = "block";
      index += 1;
      continue;
    }
    if (char === "\"") {
      state = "string";
      continue;
    }
    if (char === "'") {
      state = "char";
      continue;
    }
    if (char === "{") {
      depth += 1;
      continue;
    }
    if (char === "}") {
      depth -= 1;
      if (depth === 0) return index;
    }
  }
  throw new Error(`unbalanced block starting at offset ${open}`);
}

function splitTopLevel(body: string): string[] {
  const parts: string[] = [];
  const stack: string[] = [];
  let start = 0;
  let state: "code" | "line" | "block" | "string" | "char" = "code";
  for (let index = 0; index < body.length; index += 1) {
    const char = body[index];
    const next = body[index + 1];
    if (state === "line") {
      if (char === "\n") state = "code";
      continue;
    }
    if (state === "block") {
      if (char === "*" && next === "/") {
        state = "code";
        index += 1;
      }
      continue;
    }
    if (state === "string" || state === "char") {
      if (char === "\\") {
        index += 1;
        continue;
      }
      if ((state === "string" && char === "\"") || (state === "char" && char === "'")) {
        state = "code";
      }
      continue;
    }
    if (char === "/" && next === "/") {
      state = "line";
      index += 1;
      continue;
    }
    if (char === "/" && next === "*") {
      state = "block";
      index += 1;
      continue;
    }
    if (char === "\"") {
      state = "string";
      continue;
    }
    if (char === "'") {
      state = "char";
      continue;
    }
    if (char === "{" || char === "(" || char === "[") {
      stack.push(char);
      continue;
    }
    if (char === "}" || char === ")" || char === "]") {
      stack.pop();
      continue;
    }
    if (char === "," && stack.length === 0) {
      parts.push(body.slice(start, index).trim());
      start = index + 1;
    }
  }
  parts.push(body.slice(start).trim());
  return parts.filter((part) => part.length > 0);
}

function decodeCppString(raw: string): string {
  const escapes: Record<string, string> = {
    n: "\n",
    r: "\r",
    t: "\t",
    "0": "\0",
    "\"": "\"",
    "\\": "\\",
  };
  return raw.replace(/\\(.)/g, (_, char: string) => escapes[char] ?? char);
}

function parseStringLiterals(field: string): string[] {
  const values: string[] = [];
  for (let index = 0; index < field.length; index += 1) {
    if (field[index] !== "\"") continue;
    let raw = "";
    index += 1;
    while (index < field.length) {
      const char = field[index];
      if (char === "\\") {
        raw += char;
        index += 1;
        if (index < field.length) raw += field[index];
        index += 1;
        continue;
      }
      if (char === "\"") break;
      raw += char;
      index += 1;
    }
    values.push(decodeCppString(raw));
  }
  return values;
}

function parseFlags(field: string): string[] {
  const flags: string[] = [];
  for (const match of field.matchAll(/tool_flags::(k[A-Za-z0-9_]+)/g)) {
    if (!flags.includes(match[1])) flags.push(match[1]);
  }
  return flags;
}

function countTopLevelEntries(body: string): number {
  let count = 0;
  let depth = 0;
  let state: "code" | "line" | "block" | "string" | "char" = "code";
  for (let index = 0; index < body.length; index += 1) {
    const char = body[index];
    const next = body[index + 1];
    if (state === "line") {
      if (char === "\n") state = "code";
      continue;
    }
    if (state === "block") {
      if (char === "*" && next === "/") {
        state = "code";
        index += 1;
      }
      continue;
    }
    if (state === "string" || state === "char") {
      if (char === "\\") {
        index += 1;
        continue;
      }
      if ((state === "string" && char === "\"") || (state === "char" && char === "'")) {
        state = "code";
      }
      continue;
    }
    if (char === "/" && next === "/") {
      state = "line";
      index += 1;
      continue;
    }
    if (char === "/" && next === "*") {
      state = "block";
      index += 1;
      continue;
    }
    if (char === "\"") {
      state = "string";
      continue;
    }
    if (char === "'") {
      state = "char";
      continue;
    }
    if (char === "{") {
      if (depth === 0) count += 1;
      depth += 1;
      continue;
    }
    if (char === "}") {
      depth -= 1;
    }
  }
  return count;
}

function parseParamCounts(text: string): Map<string, number> {
  const counts = new Map<string, number>();
  for (const match of text.matchAll(/const\s+std::vector<ParamSpec>\s+([A-Za-z_]\w*)\s*=\s*\{/g)) {
    const open = (match.index ?? 0) + match[0].length - 1;
    const end = findBlockEnd(text, open);
    counts.set(match[1], countTopLevelEntries(text.slice(open + 1, end)));
  }
  return counts;
}

type AliasMapping = { name: string; canonical: string; action: string };

function aliasActionOf(text: string): string {
  const helper = text.match(/_with_action\([^,]+,\s*"([^"]+)"\)/);
  if (helper) return helper[1];
  const assignment = text.match(/\["action"\]\s*=\s*mcp::JsonValue\("([^"]+)"\)/);
  return assignment ? assignment[1] : "";
}

function parseAliasMappings(text: string): AliasMapping[] {
  const mappings: AliasMapping[] = [];
  let searchFrom = 0;
  while (true) {
    const index = text.indexOf("ToolAlias{", searchFrom);
    if (index < 0) break;
    const open = index + "ToolAlias".length;
    const end = findBlockEnd(text, open);
    const fields = splitTopLevel(text.slice(open + 1, end));
    if (fields.length >= 2) {
      const names = parseStringLiterals(fields[0]);
      const canonicals = parseStringLiterals(fields[1]);
      if (names.length === 0 || canonicals.length === 0) {
        throw new Error(`ToolAlias at offset ${index} has no string name/canonical`);
      }
      mappings.push({
        name: names[0],
        canonical: canonicals[0],
        action: fields.length >= 3 ? aliasActionOf(fields[2]) : "",
      });
    }
    searchFrom = end + 1;
  }
  for (const match of text.matchAll(
    /[A-Za-z_]\w*_alias\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"(?:\s*,\s*"([^"]+)")?/g,
  )) {
    mappings.push({
      name: match[1],
      canonical: match[2],
      action: match[3] ?? "",
    });
  }
  for (const match of text.matchAll(
    /physics_query_alias\s*\(\s*"([^"]+)"\s*,\s*[23]\s*,\s*"([^"]+)"/g,
  )) {
    mappings.push({
      name: match[1],
      canonical: "physics_query",
      action: match[2],
    });
  }
  return mappings;
}

function parseToolSpecs(
  text: string,
  source: string,
  domain: string,
  paramCounts: Map<string, number>,
): ToolRecord[] {
  const records: ToolRecord[] = [];
  let searchFrom = 0;
  while (true) {
    const index = text.indexOf("ToolSpec{", searchFrom);
    if (index < 0) break;
    const open = index + "ToolSpec".length;
    const end = findBlockEnd(text, open);
    const fields = splitTopLevel(text.slice(open + 1, end));
    const line = text.slice(0, index).split("\n").length;
    if (fields.length < 8) {
      throw new Error(`${source}:${line}: ToolSpec block has ${fields.length} fields, expected at least 8`);
    }
    const names = parseStringLiterals(fields[0]);
    const categories = parseStringLiterals(fields[2]);
    if (names.length === 0 || categories.length === 0) {
      throw new Error(`${source}:${line}: ToolSpec name/category is not a string literal`);
    }
    const paramsField = fields[6].trim();
    let paramCount: number | null = null;
    if (paramsField.startsWith("{")) {
      const paramsOpen = fields[6].indexOf("{");
      const paramsEnd = findBlockEnd(fields[6], paramsOpen);
      paramCount = countTopLevelEntries(fields[6].slice(paramsOpen + 1, paramsEnd));
    } else if (paramsField.length > 0) {
      const resolved = paramCounts.get(paramsField);
      if (resolved === undefined) {
        throw new Error(`${source}:${line}: unknown ParamSpec vector ${paramsField}`);
      }
      paramCount = resolved;
    }
    const sideEffectMatch = fields[4].match(/SideEffect::([A-Za-z0-9_]+)/);
    records.push({
      name: names[0],
      category: categories[0],
      domain,
      side_effect: sideEffectMatch ? sideEffectMatch[1] : "None",
      flags: parseFlags(fields[5]),
      param_count: paramCount,
      has_action_schema:
        /[A-Za-z_]\w*_action_spec\(\s*$/.test(
          text.slice(Math.max(0, index - 160), index),
        ) ||
        fields.some((field) => field.includes("build_action_schema(")),
      source,
      line,
    });
    searchFrom = end + 1;
  }
  return records;
}

function parseSystemSpecs(text: string): { meta: ToolRecord[]; system: ToolRecord[] } {
  const meta: ToolRecord[] = [];
  const system: ToolRecord[] = [];
  const source = relativeToRoot(REGISTER_ALL);
  let searchFrom = 0;
  while (true) {
    const index = text.indexOf("ToolSpec spec{", searchFrom);
    if (index < 0) break;
    const open = index + "ToolSpec spec".length;
    const end = findBlockEnd(text, open);
    const fields = splitTopLevel(text.slice(open + 1, end));
    const line = text.slice(0, index).split("\n").length;
    if (fields.length < 4) {
      throw new Error(`${source}:${line}: system ToolSpec block has ${fields.length} fields, expected at least 4`);
    }
    const names = parseStringLiterals(fields[0]);
    const categories = parseStringLiterals(fields[2]);
    if (names.length === 0 || categories.length === 0) {
      throw new Error(`${source}:${line}: system ToolSpec name/category is not a string literal`);
    }
    const nextIndex = text.indexOf("ToolSpec spec{", end + 1);
    const tail = text.slice(end + 1, nextIndex < 0 ? text.length : nextIndex);
    const sideEffectMatch = tail.match(/spec\.side_effect\s*=\s*SideEffect::([A-Za-z0-9_]+)/);
    const flagsMatch = tail.match(/spec\.flags\s*=\s*([^;]+);/);
    const record: ToolRecord = {
      name: names[0],
      category: categories[0],
      domain: "System",
      side_effect: sideEffectMatch ? sideEffectMatch[1] : "None",
      flags: flagsMatch ? parseFlags(flagsMatch[1]) : [],
      param_count: null,
      source,
      line,
    };
    if (record.flags.includes("kMeta")) {
      record.domain = "Meta";
      meta.push(record);
    } else {
      system.push(record);
    }
    searchFrom = end + 1;
  }
  return { meta, system };
}

function loadPreviousMatrixTools(): Set<string> {
  if (!fs.existsSync(MATRIX_PATH)) {
    return new Set();
  }
  const previous = JSON.parse(fs.readFileSync(MATRIX_PATH, "utf8")) as {
    rows?: Array<{ old_tool?: string }>;
  };
  const names = new Set<string>();
  for (const row of previous.rows ?? []) {
    if (typeof row.old_tool === "string" && row.old_tool.length > 0) {
      names.add(row.old_tool);
    }
  }
  return names;
}

function countValues<T extends string>(values: T[]): Record<string, number> {
  const counts: Record<string, number> = {};
  for (const value of values) counts[value] = (counts[value] ?? 0) + 1;
  return counts;
}

function buildAudit() {
  const dirs = fs
    .readdirSync(DOMAINS_DIR, { withFileTypes: true })
    .filter((entry) => entry.isDirectory())
    .map((entry) => entry.name)
    .sort();
  const unknownDomains = dirs.filter((name) => DOMAIN_LABELS[name] === undefined);
  if (unknownDomains.length > 0) {
    throw new Error(`unknown domains without target labels: ${unknownDomains.join(", ")}`);
  }

  const domainTools: ToolRecord[] = [];
  const domainOrder: string[] = [];
  const aliasMappings: AliasMapping[] = [];
  for (const dir of dirs) {
    const file = path.join(DOMAINS_DIR, dir, `${dir}_tools.hpp`);
    const source = relativeToRoot(file);
    const text = fs.readFileSync(file, "utf8");
    const paramCounts = parseParamCounts(text);
    const records = parseToolSpecs(text, source, DOMAIN_LABELS[dir], paramCounts);
    domainOrder.push(DOMAIN_LABELS[dir]);
    domainTools.push(...records);
    aliasMappings.push(...parseAliasMappings(text));
  }

  const registerText = fs.readFileSync(REGISTER_ALL, "utf8");
  aliasMappings.push(...parseAliasMappings(registerText));
  const uniqueAliasMappings = [
    ...new Map(aliasMappings.map((mapping) => [mapping.name, mapping])).values(),
  ];
  const aliasNames = uniqueAliasMappings.map((mapping) => mapping.name);
  const { meta, system } = parseSystemSpecs(registerText);
  const allTools = [...domainTools, ...system, ...meta];
  const names = allTools.map((tool) => tool.name);
  const duplicateNames = names.filter((name, index) => names.indexOf(name) !== index);
  if (duplicateNames.length > 0) {
    throw new Error(`duplicate tool names: ${[...new Set(duplicateNames)].join(", ")}`);
  }
  if (meta.length !== 5) {
    throw new Error(`expected 5 meta tools, found ${meta.length}`);
  }
  if (system.length !== 1) {
    throw new Error(`expected 1 system tool, found ${system.length}`);
  }
  const knownMetaNames = new Set(meta.map((tool) => tool.name));
  const missingMetaSchemas = [...META_TOOLS_WITH_PARAMS].filter(
    (name) => !knownMetaNames.has(name),
  );
  if (missingMetaSchemas.length > 0) {
    throw new Error(`meta schema set references missing tools: ${missingMetaSchemas.join(", ")}`);
  }
  const schemaCount = (tool: ToolRecord) =>
    (tool.param_count ?? 0) > 0 || tool.has_action_schema === true;
  const domainWithParams = domainTools.filter(schemaCount).length;
  const systemWithParams = system.filter(schemaCount).length;
  const metaWithParams = meta.filter((tool) => META_TOOLS_WITH_PARAMS.has(tool.name)).length;
  const schemaNonEmpty = domainWithParams + systemWithParams + metaWithParams;
  const duplicateAliases = aliasNames.filter(
    (name, index) => aliasNames.indexOf(name) !== index,
  );
  if (duplicateAliases.length > 0) {
    throw new Error(`duplicate aliases: ${[...new Set(duplicateAliases)].join(", ")}`);
  }

  const known = new Set(names);
  const previousMatrixTools = loadPreviousMatrixTools();
  const missingPrune = PRUNE_ROWS.filter(
    (row) =>
      !known.has(row.old_tool) &&
      !previousMatrixTools.has(row.old_tool) &&
      !PREVIOUS_META_TOOLS.has(row.old_tool),
  );
  if (missingPrune.length > 0) {
    throw new Error(`prune rows reference unknown tools: ${missingPrune.map((row) => row.old_tool).join(", ")}`);
  }

  const sideEffectReview = domainTools.filter(
    (tool) =>
      tool.side_effect === "None" &&
      MUTATION_PREFIXES.some((prefix) => tool.name.startsWith(prefix)),
  );

  const byDomain = Object.fromEntries(
    domainOrder.map((label) => [label, domainTools.filter((tool) => tool.domain === label).length]),
  );

  return {
    schema: 1,
    source: {
      domains: "src/tools/domains/*/*_tools.hpp",
      register_all: relativeToRoot(REGISTER_ALL),
    },
    catalog_total: allTools.length,
    domain_total: domainTools.length,
    meta_total: meta.length,
    system_total: system.length,
    domain_count: dirs.length,
    by_domain: byDomain,
    by_category: countValues(allTools.map((tool) => tool.category)),
    domain_by_category: countValues(domainTools.map((tool) => tool.category)),
    by_side_effect: countValues(allTools.map((tool) => tool.side_effect)),
    domain_by_side_effect: countValues(domainTools.map((tool) => tool.side_effect)),
    strict_redundant_count: PRUNE_ROWS.length,
    alias_count: aliasNames.length,
    alias_names: [...aliasNames].sort(),
    alias_mappings: [...uniqueAliasMappings].sort((a, b) =>
      a.name.localeCompare(b.name),
    ),
    schema_non_empty: schemaNonEmpty,
    schema_empty: allTools.length - schemaNonEmpty,
    tool_names: names,
    tools: domainTools,
    meta_tools: meta,
    system_tools: system,
    side_effect_review: sideEffectReview.map((tool) => ({
      name: tool.name,
      domain: tool.domain,
      side_effect: tool.side_effect,
      source: tool.source,
      line: tool.line,
    })),
  };
}

export function getCanonicalCatalog(): string[] {
  return [...buildAudit().tool_names];
}

function buildTarget(audit: ReturnType<typeof buildAudit>) {
  const domains = Object.keys(audit.by_domain).map((name) => {
    const definition = DOMAIN_TARGETS[name];
    if (!definition) throw new Error(`missing target definition for domain ${name}`);
    return {
      name,
      current: audit.by_domain[name],
      target: definition.target,
      plan_target: definition.plan_target,
      merge_groups: definition.merge_groups,
      ...(definition.note ? { note: definition.note } : {}),
    };
  });
  const effectiveTotal = domains.reduce((sum, entry) => sum + entry.target, 0);
  const planTotal = domains.reduce((sum, entry) => sum + entry.plan_target, 0);
  const metaTarget = 5;
  const systemTarget = 1;
  const catalogExpected = effectiveTotal + NEW_TOOLS.length + systemTarget + metaTarget;
  return {
    schema: 1,
    source: "src/tools/domains/*/*_tools.hpp",
    catalog_current: audit.catalog_total,
    catalog_target: 270,
    catalog_expected: catalogExpected,
    catalog_target_range: [250, 300],
    domain_current_total: audit.domain_total,
    domain_target_total: effectiveTotal,
    planned_domain_target_total: planTotal,
    net_new_total: NEW_TOOLS.length,
    strict_prune_count: PRUNE_ROWS.length,
    meta_current: audit.meta_total,
    meta_target: metaTarget,
    system_current: audit.system_total,
    system_target: systemTarget,
    hot_names: HOT_NAMES,
    domains,
    new_tools: NEW_TOOLS,
    alias_placeholders: PRUNE_ROWS.map((row) => ({
      old_tool: row.old_tool,
      canonical: row.new_tool,
      action: row.action,
      arg_adapt: row.arg_adapt,
      result_projection: row.result_projection,
      status: "planned",
    })),
    notes: [
      "D-08 的 Debug 当前计数为 16，源码按域文件实数为 15；D-07 删除 6 项后有效目标为 9。",
      "D-08 的域目标合计 258 未计入净新增 6 项；本文件按 257 + 6 + 1 + 5 推导 catalog_expected=269。",
      "catalog_target=270 保留为计划声明值；执行期允许 250–300。",
    ],
  };
}

type MatrixRow = {
  old_tool: string;
  domain: string;
  category: string;
  side_effect: string;
  new_tool: string;
  action: string;
  alias: boolean;
  status: string;
  arg_adapt: string;
  result_projection: string;
};

function buildCapabilityMatrix(audit: ReturnType<typeof buildAudit>) {
  const pruneByTool = new Map(PRUNE_ROWS.map((row) => [row.old_tool, row]));
  const aliasByTool = new Map(
    audit.alias_mappings.map((mapping) => [mapping.name, mapping]),
  );
  const toRow = (tool: ToolRecord): MatrixRow => {
    const prune = pruneByTool.get(tool.name);
    return {
      old_tool: tool.name,
      domain: tool.domain,
      category: tool.category,
      side_effect: tool.side_effect,
      new_tool: prune ? prune.new_tool : tool.name,
      action: prune ? prune.action : "",
      alias: prune !== undefined,
      status: prune ? "mapped" : "kept",
      arg_adapt: prune ? prune.arg_adapt : "",
      result_projection: prune ? prune.result_projection : "",
    };
  };
  let previousRows: MatrixRow[] = [];
  if (fs.existsSync(MATRIX_PATH)) {
    const previous = JSON.parse(fs.readFileSync(MATRIX_PATH, "utf8")) as {
      rows?: MatrixRow[];
    };
    if (Array.isArray(previous.rows)) {
      previousRows = previous.rows;
    }
  }
  const currentByName = new Map(audit.tools.map((tool) => [tool.name, tool]));
  const rows = previousRows.length > 0
    ? previousRows.map((previousRow) => {
        const current = currentByName.get(previousRow.old_tool);
        if (current) {
          return toRow(current);
        }
        const prune = pruneByTool.get(previousRow.old_tool);
        const alias = aliasByTool.get(previousRow.old_tool);
        return {
          old_tool: previousRow.old_tool,
          domain: previousRow.domain,
          category: previousRow.category,
          side_effect: previousRow.side_effect,
          new_tool: prune?.new_tool ?? alias?.canonical ?? previousRow.new_tool,
          action: prune?.action ?? alias?.action ?? previousRow.action,
          alias: prune !== undefined || alias !== undefined || previousRow.alias,
          status: prune || alias
            ? "mapped"
            : previousRow.status === "mapped"
              ? "mapped"
              : "missing",
          arg_adapt: prune?.arg_adapt ?? (alias ? "alias" : previousRow.arg_adapt),
          result_projection:
            prune?.result_projection ??
            (alias ? "alias" : previousRow.result_projection),
        };
      })
    : audit.tools.map(toRow);
  const knownRows = new Set(rows.map((row) => row.old_tool));
  for (const tool of audit.tools) {
    if (!knownRows.has(tool.name)) {
      rows.push(toRow(tool));
    }
  }
  const metaRows = [...audit.system_tools, ...audit.meta_tools].map(toRow);
  const allRows = [...rows, ...metaRows];
  return {
    schema: 1,
    source: "src/tools/domains/*/*_tools.hpp",
    row_count: rows.length,
    unique_tool_count: new Set(rows.map((row) => row.old_tool)).size,
    catalog_row_count: allRows.length,
    catalog_unique_tool_count: new Set(allRows.map((row) => row.old_tool)).size,
    strict_prune_count: PRUNE_ROWS.length,
    domain_mapped_count: rows.filter((row) => row.status === "mapped").length,
    domain_kept_count: rows.filter((row) => row.status === "kept").length,
    domain_missing_count: rows.filter((row) => row.status === "missing").length,
    meta_mapped_count: metaRows.filter((row) => row.status === "mapped").length,
    meta_kept_count: metaRows.filter((row) => row.status === "kept").length,
    mapped_count: allRows.filter((row) => row.status === "mapped").length,
    kept_count: allRows.filter((row) => row.status === "kept").length,
    rows,
    meta_rows: metaRows,
  };
}

function buildReport(
  audit: ReturnType<typeof buildAudit>,
  matrix: ReturnType<typeof buildCapabilityMatrix>,
) {
  return {
    schema: 1,
    source: audit.source,
    catalog: {
      total: audit.catalog_total,
      domain: audit.domain_total,
      meta: audit.meta_total,
      system: audit.system_total,
      domain_count: audit.domain_count,
    },
    alias_count: audit.alias_count,
    parameter_schema: {
      non_empty: audit.schema_non_empty,
      empty: audit.schema_empty,
    },
    capability_matrix: {
      row_count: matrix.row_count,
      catalog_row_count: matrix.catalog_row_count,
      mapped_count: matrix.mapped_count,
      kept_count: matrix.kept_count,
      missing_count: matrix.catalog_row_count - matrix.mapped_count - matrix.kept_count,
      meta_row_count: matrix.meta_rows.length,
    },
    by_domain: audit.by_domain,
    by_side_effect: audit.by_side_effect,
    domain_by_side_effect: audit.domain_by_side_effect,
  };
}

function sortedCounts(counts: Record<string, number>): Array<[string, number]> {
  return Object.entries(counts).sort(
    (a, b) => b[1] - a[1] || a[0].localeCompare(b[0]),
  );
}

function countTable(
  title: string,
  rows: Array<[string, number | string]>,
): string[] {
  return [
    `## ${title}`,
    "",
    "| Metric | Value |",
    "| --- | ---: |",
    ...rows.map(([name, value]) => `| ${name} | ${value} |`),
    "",
  ];
}

function renderReportText(report: ReturnType<typeof buildReport>): string {
  const { catalog, parameter_schema: schema, capability_matrix: matrix } = report;
  const lines = [
    "# GDA tool surface audit report",
    "",
    `- source: ${report.source.domains}`,
    `- register_all: ${report.source.register_all}`,
    "",
    ...countTable("Catalog", [
      ["catalog_total", catalog.total],
      ["domain_total", catalog.domain],
      ["meta_total", catalog.meta],
      ["system_total", catalog.system],
      ["domain_count", catalog.domain_count],
      ["alias_count", report.alias_count],
      ["schema_non_empty", schema.non_empty],
      ["schema_empty", schema.empty],
    ]),
    ...countTable("Capability matrix", [
      ["row_count", matrix.row_count],
      ["catalog_row_count", matrix.catalog_row_count],
      ["mapped_count", matrix.mapped_count],
      ["kept_count", matrix.kept_count],
      ["missing_count", matrix.missing_count],
      ["meta_row_count", matrix.meta_row_count],
    ]),
    ...countTable("Side effects (catalog)", sortedCounts(report.by_side_effect)),
    ...countTable(
      "Side effects (domain tools)",
      sortedCounts(report.domain_by_side_effect),
    ),
    "## Tools by domain",
    "",
    "| Domain | Count |",
    "| --- | ---: |",
    ...Object.entries(report.by_domain).map(([name, count]) => `| ${name} | ${count} |`),
    "",
  ];
  return lines.join("\n");
}

function writeJson(target: string, value: unknown): void {
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.writeFileSync(target, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function writeText(target: string, text: string): void {
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.writeFileSync(target, text, "utf8");
}

function printSummary(audit: ReturnType<typeof buildAudit>, matrixRows: number): void {
  console.log(
    `[AUDIT] catalog=${audit.catalog_total} domain=${audit.domain_total} meta=${audit.meta_total} system=${audit.system_total}`,
  );
  console.log(
    `[AUDIT] domains=${audit.domain_count} aliases=${audit.alias_count} schema_non_empty=${audit.schema_non_empty} schema_empty=${audit.schema_empty} strict_redundant=${audit.strict_redundant_count} matrix_rows=${matrixRows}`,
  );
  console.log(`[AUDIT] baseline=${relativeToRoot(BASELINE_PATH)}`);
  console.log(`[AUDIT] target=${relativeToRoot(TARGET_PATH)}`);
  console.log(`[AUDIT] matrix=${relativeToRoot(MATRIX_PATH)}`);
}

export function runToolAudit(options: ToolAuditOptions): number {
  const audit = buildAudit();
  const matrix = buildCapabilityMatrix(audit);
  if (options.report) {
    const report = buildReport(audit, matrix);
    const payload = options.json
      ? `${JSON.stringify(report, null, 2)}\n`
      : renderReportText(report);
    if (options.out !== null) {
      const outPath = path.resolve(options.out);
      writeText(outPath, payload);
      process.stderr.write(`[AUDIT] report written to ${relativeToRoot(outPath)}\n`);
    }
    process.stdout.write(payload);
    return 0;
  }
  if (options.json) {
    process.stdout.write(`${JSON.stringify(audit, null, 2)}\n`);
    return 0;
  }
  if (options.baseline) {
    writeJson(BASELINE_PATH, audit);
    printSummary(audit, matrix.row_count);
    return 0;
  }
  if (options.target) {
    writeJson(TARGET_PATH, buildTarget(audit));
    writeJson(MATRIX_PATH, matrix);
    printSummary(audit, matrix.row_count);
    return 0;
  }
  printSummary(audit, matrix.row_count);
  return 0;
}
