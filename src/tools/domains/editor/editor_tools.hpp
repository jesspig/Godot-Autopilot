#ifndef GODOT_AUTOPILOT_EDITOR_TOOLS_HPP
#define GODOT_AUTOPILOT_EDITOR_TOOLS_HPP

#include <algorithm>
#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/editor/editor_ops.hpp"
#include "tools/domains/editor/editor_ui_actions_ops.hpp"
#include "tools/domains/editor/editor_ui_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace editor_tools {

namespace {

const std::vector<std::string> kGetEditorStateActions = {
    "selection", "root", "open", "unsaved", "playing", "info"};

const std::vector<ParamSpec> kGetEditorStateParams = {};

const std::vector<std::string> kGetEditorUiActions = {
    "elements", "hit", "geometry", "scene_tree_items"};

const std::vector<ParamSpec> kGetEditorUiParams = {
    {"query", "string", "elements: case-insensitive substring filter matched against path, name, text, tooltip and placeholder", false},
    {"type_filter", "string", "elements: exact class name to restrict results to, e.g. 'Button' or 'LineEdit'", false},
    {"interactive_only", "boolean", "elements: keep only actionable controls such as buttons, line edits, trees and sliders (default: false)", false},
    {"max_elements", "integer", "elements: maximum number of elements to return, an integer from 1 to 1000 (default: 100); the result reports truncated when the cap is hit", false},
    {"position", "object", "hit: point to test, object with numeric x and y fields, in pixels relative to the client area of window_id", false},
    {"window_id", "integer", "hit: window to test against (default: 0 = main editor window), e.g. an id from action=elements", false},
    {"max_results", "integer", "hit: maximum number of hit controls to return, an integer from 1 to 64 (default: 10)", false},
    {"include_scene_nodes", "boolean", "hit: append scene context (default: false): scene_tree_item, the scene-tree row under the point when a row is hit, and scene_nodes, the topmost-first scene node paths with name/type under the point inside the 2D editor viewport, plus scene_node_count; scene_tree_error/scene_nodes_error report unavailable context", false},
    {"viewport", "string", "geometry: viewport to report, '2d' (default) or '3d'", false},
    {"index", "integer", "geometry: 3D viewport index, 0-3 (default: 0); ignored when viewport='2d'", false},
    {"filter", "string", "scene_tree_items: case-insensitive substring filter matched against each row's scene-relative path or name", false},
    {"selected_only", "boolean", "scene_tree_items: keep only rows currently selected in the scene tree (default: false)", false},
    {"max_items", "integer", "scene_tree_items: maximum number of rows to return, an integer from 1 to 1000 (default: 200); the result reports truncated when the cap is hit", false},
};

const std::vector<std::string> kGetEditorFilesActions = {"tree", "status",
                                                         "scan"};

const std::vector<ParamSpec> kGetEditorFilesParams = {
    {"path", "string", "tree: directory path to root the tree at, e.g. 'res://scenes' or 'res://'; when omitted the whole project tree is returned. An invalid path returns null", false},
    {"depth", "integer", "tree: maximum depth below the requested root (default: 12); deeper levels are pruned and the result reports truncated with max_depth", false},
    {"prefix", "string", "tree: subtree root filter, e.g. 'res://scenes'; takes precedence over path when both are given", false},
    {"filter", "string", "tree: case-insensitive substring filter matched against each entry's name or path; non-matching leaves are pruned while ancestors of matches are kept, and the result reports filtered", false},
};

const std::vector<std::string> kEditorSceneControlActions = {
    "create", "open", "close", "reload", "play", "stop"};

const std::vector<ParamSpec> kEditorSceneControlParams = {
    {"type", "string", "create: Godot class name of the root node to create, e.g. 'Node2D' or 'Node3D'; must be a Node subclass, defaults to 'Node'", false},
    {"name", "string", "create: name of the new root node, defaults to 'NewRoot'", false},
    {"close_current", "boolean", "create: close the current scene tab first if it has no unsaved changes (default: false; errors if the current scene is unsaved). Only the current tab is closed: with multiple scene tabs open, repeat action=close until no scene is open, otherwise create fails fast without entering the switch wait", false},
    {"timeout_ms", "integer", "create: maximum time in milliseconds to wait for the editor to observe the new scene root after add_root_node (default: 2000, min: 50, max: 30000); on timeout the call errors with waited_ms, timeout_ms, node_released and editor_state diagnostics", false},
    {"path", "string", "open: path to the scene file to open, e.g. 'res://game.tscn'; errors when the current scene has unsaved changes", false},
    {"reload_if_open", "boolean", "open: when the requested scene is already the edited scene, reload it from disk instead of short-circuiting (default: false); discards unsaved changes, so save first when needed", false},
    {"scene_path", "string", "reload: scene file path to reload from disk, e.g. 'res://game.tscn'; when omitted the currently edited scene is reloaded", false},
    {"discard_unsaved", "boolean", "reload: explicit permission to discard unsaved changes in the target scene; required together with confirm=true when the scene has unsaved changes, otherwise reload is rejected with confirmation_required", false},
    {"confirm", "boolean", "reload: explicit confirmation together with discard_unsaved=true that unsaved changes may be discarded; the in-memory scene is snapshotted to user:// before reload when possible", false},
};

const std::vector<std::string> kSaveEditorSceneActions = {"save", "save_as",
                                                           "save_all"};

const std::vector<ParamSpec> kSaveEditorSceneParams = {
    {"path", "string", "save_as: target file path for the save; must normalize to a res:// path below the project root and end with .tscn or .scn; missing parent directories are created automatically", false},
    {"overwrite", "boolean", "save_as: allow replacing an existing target file; without it an existing target is rejected with file_exists", false},
    {"confirm", "boolean", "save_as: explicit confirmation, required together with overwrite=true when the target already exists", false},
    {"include_hash", "boolean", "save/save_as: attach FNV-1a structural hashes of the memory tree and the on-disk file (memory_hash/disk_hash), computed over the sorted node-path sets; off by default. Node paths only — properties, script references and file bytes do not participate, so attaching a script or changing properties leaves the hash unchanged while adding/removing nodes changes it; this is a structure check, not a content proof (read the file back to verify content; script writes carry their own content_hash over the read-back bytes)", false},
    {"include_paths", "boolean", "save/save_as: always attach the missing_paths list even when node counts match (default: attached only on mismatch)", false},
};

const std::vector<std::string> kEditorSelectionSetActions = {"set",
                                                              "select_tree"};

const std::vector<ParamSpec> kEditorSelectionSetParams = {
    {"paths", "array", "set: array of node paths in the edited scene to select, e.g. ['Player', 'Level1/Enemy'] or absolute like '/root/Level1/Enemy'; the previous selection is cleared first and unresolvable paths are skipped", false},
    {"path", "string", "select_tree: node path in the edited scene, scene-relative such as 'Player' or 'Level1/Enemy', or absolute such as '/root/Level1/Enemy'; the previous selection is cleared first unless add is true", false},
    {"add", "boolean", "select_tree: true appends the node to the current selection instead of replacing it (default: false)", false},
    {"inspect", "boolean", "select_tree: open the node in the Inspector through EditorInterface.edit_node (default: true)", false},
    {"focus", "boolean", "select_tree: scroll the scene tree to the node row when the row is found (default: true)", false},
};

const std::vector<std::string> kEditorUndoActions = {
    "create", "add_do", "add_undo", "commit", "undo", "redo", "history",
    "clear"};

const std::vector<ParamSpec> kEditorUndoParams = {
    {"name", "string", "create: human-readable name shown in the editor's undo history, e.g. 'Move player'; group related changes under one name so they revert together", false},
    {"node_path", "string", "add_do/add_undo: path to the node whose method will be called, e.g. 'Player' or '/root/Player'; resolved against the edited scene root", false},
    {"method", "string", "add_do/add_undo: name of the node method to call, e.g. 'set_position' or 'queue_free'", false},
    {"value", "json", "add_do/add_undo: optional serialized JSON value passed as the single method argument; omit for methods without arguments", false},
    {"side", "string", "add_do/add_undo: optional branch marker, 'do' for add_do and 'undo' for add_undo; a mismatching value is rejected so a merged call cannot silently swap the branch", false},
    {"increase_version", "boolean", "clear: bump the undo version after clearing the global history (default: true); false keeps the version so dependent editors are not invalidated", false},
};

const std::vector<std::string> kScriptEditorActions = {
    "open", "goto", "list_open", "current", "unsaved", "save_all", "reload_open",
    "close"};

const std::vector<ParamSpec> kScriptEditorParams = {
    {"path", "string", "open/goto/close: script file path, e.g. 'res://scripts/player.gd'; goto without path jumps inside the current script; close discards unsaved changes", false},
    {"line", "integer", "open/goto: 1-based line number; open defaults to -1 (keep the current caret) and goto requires an integer >= 1", false},
    {"column", "integer", "open: 1-based column number (default 0 keeps the current column)", false},
    {"grab_focus", "boolean", "open: focus the script editor after opening (default: true)", false},
};

const std::vector<std::string> kEditorSessionActions = {
    "get", "mark_unsaved", "play_main", "play_custom"};

const std::vector<ParamSpec> kEditorSessionParams = {
    {"path", "string", "play_custom: scene file path to run, e.g. 'res://levels/level1.tscn'; local scenes are run straight from the editor", false},
};

const std::vector<std::string> kEditorInspectActions = {
    "inspect_object", "edit_resource", "edit_node", "selected_path",
    "expand_all", "collapse_all"};

const std::vector<ParamSpec> kEditorInspectParams = {
    {"resource_path", "string", "inspect_object/edit_resource: resource file to open in the Inspector, e.g. 'res://player.tres'; loaded through ResourceLoader and errors when missing or unloadable", false},
    {"node_path", "string", "inspect_object/edit_node: node path in the edited scene, e.g. 'Player' or 'Level1/Enemy'; resolved against the edited scene root", false},
    {"for_property", "string", "inspect_object: optional property name to reveal in the Inspector, e.g. 'position'; empty keeps the default object view", false},
    {"inspector_only", "boolean", "inspect_object: open the object in the Inspector without switching the main screen (default: false)", false},
};

const std::vector<std::string> kEditorFilesPanelActions = {
    "select", "selected_paths", "current_path", "current_dir", "navigate",
    "file_meta"};

const std::vector<ParamSpec> kEditorFilesPanelParams = {
    {"path", "string", "select/navigate/file_meta: file-system path, e.g. 'res://scenes/game.tscn' or 'res://scripts'; file_meta requires a file path and reports its script class metadata", false},
};

const std::vector<std::string> kEditorInfoActions = {
    "theme", "scale", "language", "paths", "multi_window", "distraction_free",
    "toast"};

const std::vector<ParamSpec> kEditorInfoParams = {
    {"message", "string", "toast: text shown in the editor toast notification", false},
    {"severity", "string", "toast: notification severity, 'info' (default), 'warning' or 'error'", false},
    {"tooltip", "string", "toast: optional tooltip shown when hovering the toast", false},
};

const std::vector<std::string> kEditorUiActionActions = {"click", "type",
                                                          "shortcut"};

const std::vector<ParamSpec> kEditorUiActionParams = {
    {"path", "string", "click/type: element path as returned by get_editor_ui action=elements or action=hit", false},
    {"button", "string", "click: mouse button to click, left, right or middle (default: left)", false},
    {"double_click", "boolean", "click: send a second press/release pair carrying the double-click flag (default: false)", false},
    {"warp", "boolean", "click: warp the physical cursor onto the element center before clicking (default: true)", false},
    {"observe", "boolean", "click/type: append a fresh editor viewport capture to the result (default: false)", false},
    {"text", "string", "type: text to write into the focused element; may contain any UTF-8 text", false},
    {"submit", "boolean", "type: append an Enter key press and release after the text (default: false)", false},
    {"shortcut", "string", "shortcut: shortcut string with '+'-separated modifiers (ctrl/control, shift, alt, meta/super) followed by the main key, e.g. 'ctrl+s', 'ctrl+shift+z' or 'f5'; the key accepts the input tool key names plus F1-F12", false},
};

const std::vector<std::string> kEditorConfigActions = {"main_scene",
                                                       "plugin_enable"};

const std::vector<ParamSpec> kEditorConfigParams = {
    {"path", "string", "main_scene: path of the scene to launch when the project runs, e.g. 'res://game.tscn'; persisted to project.godot", false},
    {"plugin", "string", "plugin_enable: name of the editor plugin to enable or disable, e.g. 'godot_autopilot'", false},
    {"enabled", "boolean", "plugin_enable: true activates the plugin, false deactivates it", false},
};

const std::vector<ParamSpec> kBuildCsharpAssemblyParams = {
    {"wait", "boolean", "When true run dotnet build synchronously via OS.execute and wait for the BuildResult with exit_code, success, errors and output_digest; when false or omitted launch via OS.create_process and return immediately with pid and started", false},
    {"timeout_ms", "integer", "Declared wait budget in milliseconds for wait=true only, an integer between 1 and 25000 (default 25000); validated to stay below the dispatch default 27000ms and the transport timeout; rejected when wait is not true", false},
    {"probe_script", "string", "Optional res:// path ending in .cs, e.g. 'res://scripts/Player.cs'; after the build the editor loads this C# script (no instantiation) and reports result.editor_assembly_probe {path,is_dotnet_editor,state,can_instantiate,base_class,hint}; state is 'available' when the script resource loads and 'unavailable' with unavailable=true when the editor is not a .NET build or the load fails (E-11)", false},
};

const std::vector<ParamSpec> kDotnetEnvironmentParams = {};

inline godot_autopilot::util::JsonValue editor_alias_action(const godot_autopilot::util::JsonValue &args,
                                          const char *action) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  if (args.IsObject()) {
    out = args;
  }
  out["action"] = godot_autopilot::util::JsonValue(action);
  return out;
}

inline godot_autopilot::util::JsonValue editor_optional_action_schema(
    const std::vector<std::string> &actions,
    const std::vector<ParamSpec> &params, const std::string &description) {
  godot_autopilot::util::JsonValue schema_value =
      schema::build_action_schema(actions, params, description);
  godot_autopilot::util::JsonValue *required = schema_value.Find("required");
  if (required != nullptr && required->IsArray()) {
    auto &items = required->GetArray();
    items.erase(std::remove_if(items.begin(), items.end(),
                               [](const godot_autopilot::util::JsonValue &value) {
                                 return value.IsString() &&
                                        value.GetString() == "action";
                               }),
                items.end());
  }
  return schema_value;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(16);
  v.push_back(make_spec_tool(ToolSpec{
      "get_editor_state",
      "Read editor session state in one action tool. action='selection' lists the nodes selected in the edited scene (name/class/path, empty array when none); action='root' returns the edited scene root {name,type,path} or null when no scene is open; action='open' lists the open scene tab paths; action='unsaved' lists scenes with unsaved changes; action='playing' reports {is_playing}; action='info' returns engine_version, project_name, main_scene, edited_root, open_scene_count and is_playing. All actions are read-only and never ask for confirmation.",
      "Editor", {"editor", "state", "selection", "root", "open", "unsaved", "playing", "info", "get_editor_selection", "get_editor_edited_scene_root"}, SideEffect::None, tool_flags::kNone,
      kGetEditorStateParams, editor_ops::handle_editor_state,
      schema::build_action_schema(kGetEditorStateActions, kGetEditorStateParams,
                                  "State to read: selection|root|open|unsaved|playing|info"),
      {
          ToolAlias{"get_editor_selection", "get_editor_state",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "selection");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"get_editor_edited_scene_root", "get_editor_state",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "root");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"selection\"}", "{\"action\":\"info\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_editor_ui",
      "Read editor UI structure and geometry in one action tool. action='elements' enumerates editor controls with semantic path, type, text, tooltip, client-area rectangle and window_id (filters: query, type_filter, interactive_only, max_elements); action='hit' returns the control chain under a client-area position (position required; optional window_id, max_results, include_scene_nodes); action='geometry' reports how the editor 2D/3D viewport maps onto the window (optional viewport, index); action='scene_tree_items' lists Scene dock rows with path/name/type/depth/selected/rect (optional filter, selected_only, max_items). Read-only; use the paths with editor_ui_action.",
      "Editor", {"editor", "ui", "elements", "hit", "geometry", "scene_tree", "scene_tree_items", "get_editor_ui_elements", "hit_test_editor_point", "get_editor_viewport_geometry", "scene_tree_items"}, SideEffect::None, tool_flags::kNone,
      kGetEditorUiParams, editor_ui_ops::handle_editor_ui,
      schema::build_action_schema(kGetEditorUiActions, kGetEditorUiParams,
                                  "UI data to read: elements|hit|geometry|scene_tree_items"),
      {
          ToolAlias{"get_editor_ui_elements", "get_editor_ui",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "elements");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"hit_test_editor_point", "get_editor_ui",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "hit");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"get_editor_viewport_geometry", "get_editor_ui",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "geometry");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"scene_tree_items", "get_editor_ui",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "scene_tree_items");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"elements\",\"query\":\"Save\",\"interactive_only\":true}",
       "{\"action\":\"scene_tree_items\",\"filter\":\"Player\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_editor_files",
      "Inspect the editor file system in one action tool. action='tree' fetches the resource directory tree rooted at path/prefix with optional depth and filter; action='status' reports {scanning,progress}; action='scan' triggers an asynchronous rescan (returns 'ok', or {skipped,reason:'already scanning'}), then poll action='status' until scanning is false. All actions are read-tier except that scan asks the editor to rescan; none require confirmation.",
      "Editor", {"editor", "filesystem", "files", "tree", "status", "scan", "get_editor_file_system_tree", "get_editor_file_system_status", "scan_editor_file_system"}, SideEffect::None, tool_flags::kNone,
      kGetEditorFilesParams, editor_ops::handle_editor_files,
      schema::build_action_schema(kGetEditorFilesActions, kGetEditorFilesParams,
                                  "File-system action: tree|status|scan"),
      {
          ToolAlias{"get_editor_file_system_tree", "get_editor_files",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "tree");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"get_editor_file_system_status", "get_editor_files",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "status");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"scan_editor_file_system", "get_editor_files",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "scan");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"tree\",\"path\":\"res://\",\"depth\":3}",
       "{\"action\":\"status\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "editor_scene_control",
      "Control the editor scene lifecycle in one action tool. action='create' makes a new scene with a root of type/name (optional close_current, timeout_ms); action='open' opens path (optional reload_if_open); action='close' closes the current tab when it has no unsaved changes; action='reload' reloads the edited scene or scene_path from disk; when the target has unsaved changes it requires discard_unsaved=true and confirm=true and snapshots the in-memory scene to user:// before reloading; action='play' launches the edited scene and reports result ok/already_playing with is_playing; action='stop' stops the running game. Every action modifies editor window state; read the follow-up state with get_editor_state.",
      "Editor", {"editor", "scene", "create", "open", "close", "reload", "play", "stop", "create_editor_scene", "open_editor_scene", "close_editor_scene", "reload_editor_scene", "play_editor_current_scene", "stop_editor_playing"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kEditorSceneControlParams, editor_ops::handle_editor_scene_control,
      schema::build_action_schema(kEditorSceneControlActions,
                                  kEditorSceneControlParams,
                                  "Scene lifecycle action: create|open|close|reload|play|stop"),
      {
          ToolAlias{"create_editor_scene", "editor_scene_control",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "create");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"open_editor_scene", "editor_scene_control",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "open");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"close_editor_scene", "editor_scene_control",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "close");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"reload_editor_scene", "editor_scene_control",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "reload");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"play_editor_current_scene", "editor_scene_control",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "play");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"stop_editor_playing", "editor_scene_control",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "stop");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"open\",\"path\":\"res://game.tscn\"}",
       "{\"action\":\"play\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "save_editor_scene",
      "Save editor scenes to disk in one action tool. action='save' (the default when action is omitted, preserving the old call shape) saves the edited scene and attaches the memory/disk receipt (optional include_hash, include_paths); a scene without a file path is written to res://<root>.tscn with a note; action='save_as' normalizes path to a res:// scene path, requires overwrite=true and confirm=true when the target exists, and reports saved/tab_path_registered plus the same receipt; action='save_all' saves every open scene tab. All actions write files; the result carries the on-disk receipt.",
      "Editor", {"editor", "scene", "save", "save_as", "save_all", "save_editor_scene_as", "save_editor_scenes"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSaveEditorSceneParams, editor_ops::handle_editor_save,
      editor_optional_action_schema(kSaveEditorSceneActions,
                                    kSaveEditorSceneParams,
                                    "Save action: save (default when omitted)|save_as|save_all"),
      {
          ToolAlias{"save_editor_scene_as", "save_editor_scene",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "save_as");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"save_editor_scenes", "save_editor_scene",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "save_all");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"save\"}",
       "{\"action\":\"save_as\",\"path\":\"res://levels/level1.tscn\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "editor_selection_set",
      "Select nodes in the edited scene in one action tool. action='set' replaces the selection with exactly paths (unresolvable entries are skipped); action='select_tree' selects path, optionally appends with add, opens it in the Inspector with inspect and scrolls the Scene dock with focus. Both actions modify editor window state; use get_editor_state action=selection to verify.",
      "Editor", {"editor", "selection", "set", "select", "scene_tree", "set_editor_selection", "select_scene_tree_node"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kEditorSelectionSetParams, editor_ops::handle_editor_selection_set,
      schema::build_action_schema(kEditorSelectionSetActions,
                                  kEditorSelectionSetParams,
                                  "Selection action: set|select_tree"),
      {
          ToolAlias{"set_editor_selection", "editor_selection_set",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "set");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"select_scene_tree_node", "editor_selection_set",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "select_tree");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"set\",\"paths\":[\"Player\"]}",
       "{\"action\":\"select_tree\",\"path\":\"Player\",\"focus\":true}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "editor_undo",
      "Drive the editor undo/redo history in one action tool. action='create' opens a named action; action='add_do' and action='add_undo' register the redo/undo method calls for node_path/method with an optional value; action='commit' closes the action so it becomes one history step. add_do/add_undo keep the do/undo branch explicit and accept an optional side marker ('do' for add_do, 'undo' for add_undo) that is rejected when it would swap the branch, so merged calls cannot reorder the pair. action='undo' and action='redo' execute one step on the global history; action='history' reports has_undo, has_redo, current_action_name and history_count without changing anything; action='clear' wipes the global history (optional increase_version). Every action except history modifies editor state; none require confirmation. Verify the result through has_undo/has_redo and get_editor_state.",
      "Editor", {"editor", "undo", "redo", "history", "clear", "create", "add_do", "add_undo", "commit", "create_editor_undo_redo_action", "add_editor_undo_redo_do", "add_editor_undo_redo_undo", "commit_editor_undo_redo"}, SideEffect::ModifiesWindow, tool_flags::kMutating | tool_flags::kUndoable,
      kEditorUndoParams, editor_ops::handle_editor_undo,
      schema::build_action_schema(kEditorUndoActions, kEditorUndoParams,
                                  "Undo history action: create|add_do|add_undo|commit|undo|redo|history|clear"),
      {
          ToolAlias{"create_editor_undo_redo_action", "editor_undo",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "create");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"add_editor_undo_redo_do", "editor_undo",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "add_do");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"add_editor_undo_redo_undo", "editor_undo",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "add_undo");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"commit_editor_undo_redo", "editor_undo",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "commit");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"create\",\"name\":\"Move player\"}",
       "{\"action\":\"add_do\",\"node_path\":\"Player\",\"method\":\"set_position\",\"value\":{\"x\":10,\"y\":20}}",
       "{\"action\":\"history\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "script_editor",
      "Open, navigate and maintain scripts in the editor's Script Editor in one action tool. action='open' opens path through EditorInterface.edit_script with optional 1-based line/column (line -1 keeps the caret) and grab_focus, then reports the resulting caret position; action='goto' jumps to a 1-based line in the current script through ScriptEditor.goto_line, or opens path at that line; action='list_open' lists open scripts with unsaved/current flags; action='current' reports the active script path, unsaved state and caret; action='unsaved' lists scripts with unsaved changes; action='save_all' saves every open script and returns saved_paths plus unsaved_after; action='reload_open' reloads open scripts from disk; action='close' closes path and discards unsaved changes. Every action modifies editor window state; none require confirmation. Read action=goto or action=current caret_line to prove the line jump.",
      "Editor", {"editor", "script", "script_editor", "open", "goto", "save", "reload", "close", "unsaved"},
      SideEffect::ModifiesWindow, tool_flags::kMutating,
      kScriptEditorParams, editor_ops::handle_script_editor,
      schema::build_action_schema(kScriptEditorActions, kScriptEditorParams,
                                  "Script editor action: open|goto|list_open|current|unsaved|save_all|reload_open|close"),
      {},
      {"{\"action\":\"open\",\"path\":\"res://scripts/player.gd\",\"line\":12,\"column\":5}",
       "{\"action\":\"goto\",\"line\":42}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "editor_session",
      "Read and drive the editable scene session in one action tool. action='get' reports every open scene tab root (name/type/scene_file_path/unsaved), the unsaved scene list, edited_root and playing {is_playing, playing_scene}; action='mark_unsaved' marks the edited scene as dirty; action='play_main' runs the project's main scene; action='play_custom' runs path. get is read-tier; mark_unsaved/play_main/play_custom modify editor window state but never ask for confirmation. With several scene tabs open, use get instead of assuming a single edited root.",
      "Editor", {"editor", "session", "scenes", "open", "unsaved", "playing", "play", "mark_unsaved"},
      SideEffect::ModifiesWindow, tool_flags::kMutating,
      kEditorSessionParams, editor_ops::handle_editor_session,
      schema::build_action_schema(kEditorSessionActions, kEditorSessionParams,
                                  "Session action: get|mark_unsaved|play_main|play_custom"),
      {},
      {"{\"action\":\"get\"}",
       "{\"action\":\"play_custom\",\"path\":\"res://levels/level1.tscn\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "editor_inspect",
      "Inspect resources and scene nodes in the editor Inspector in one action tool. action='inspect_object' opens resource_path or node_path in the Inspector (optional for_property, inspector_only); action='edit_resource' opens resource_path as an edited resource; action='edit_node' focuses node_path in the Inspector and scene dock; action='selected_path' reports the selected property path and the edited object; action='expand_all' and action='collapse_all' fold the Inspector tree. inspect_object/edit_resource/edit_node modify editor window state, selected_path/expand_all/collapse_all are read-tier; none require confirmation. inspect_editor_resource is the legacy alias of action=edit_resource.",
      "Editor", {"editor", "inspect", "inspector", "resource", "node", "selected_path", "expand_all", "collapse_all", "inspect_editor_resource"},
      SideEffect::ModifiesWindow, tool_flags::kMutating,
      kEditorInspectParams, editor_ops::handle_editor_inspect,
      schema::build_action_schema(kEditorInspectActions, kEditorInspectParams,
                                  "Inspector action: inspect_object|edit_resource|edit_node|selected_path|expand_all|collapse_all"),
      {
          ToolAlias{"inspect_editor_resource", "editor_inspect",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "edit_resource");
                    },
                    [](const godot_autopilot::util::JsonValue &result) -> godot_autopilot::util::JsonValue {
                      if (result.Find("error") != nullptr)
                        return result;
                      godot_autopilot::util::JsonValue legacy(godot_autopilot::util::JsonValue::object_tag);
                      legacy["result"] = godot_autopilot::util::JsonValue("ok");
                      return legacy;
                    },
                    "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"edit_node\",\"node_path\":\"Player\"}",
       "{\"action\":\"edit_resource\",\"resource_path\":\"res://player.tres\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "editor_files",
      "Locate and inspect files in the editor FileSystem dock in one action tool. action='select' selects path in the dock and reports selected_paths; action='selected_paths' lists the dock selection; action='current_path' reports the focused file path; action='current_dir' reports the focused directory; action='navigate' scrolls the dock to path; action='file_meta' reports type, script_class, extends and import_valid for path from the editor file-system tree. select/navigate modify editor window state, selected_paths/current_path/current_dir/file_meta are read-tier; none require confirmation. get_editor_files action=tree still lists the raw resource tree.",
      "Editor", {"editor", "files", "filesystem", "select", "navigate", "file_meta", "current_path", "current_dir", "selected_paths"},
      SideEffect::ModifiesWindow, tool_flags::kMutating,
      kEditorFilesPanelParams, editor_ops::handle_editor_files_panel,
      schema::build_action_schema(kEditorFilesPanelActions,
                                  kEditorFilesPanelParams,
                                  "FileSystem dock action: select|selected_paths|current_path|current_dir|navigate|file_meta"),
      {},
      {"{\"action\":\"file_meta\",\"path\":\"res://scripts/player.gd\"}",
       "{\"action\":\"navigate\",\"path\":\"res://scenes\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "editor_info",
      "Read editor-level information and push a toast in one action tool. action='theme' reports the editor theme base scale, default font flags and type count; action='scale' reports the editor UI scale; action='language' reports the editor UI language; action='paths' reports the EditorPaths data/config/cache/project-settings directories; action='multi_window' and action='distraction_free' report those editor modes; action='toast' pushes an EditorToaster notification with message, optional severity (info/warning/error) and tooltip. Only toast shows an alert; no action requires confirmation.",
      "Editor", {"editor", "info", "theme", "scale", "language", "paths", "multi_window", "distraction_free", "toast"},
      SideEffect::ShowsAlert, tool_flags::kMutating,
      kEditorInfoParams, editor_ops::handle_editor_info,
      schema::build_action_schema(kEditorInfoActions, kEditorInfoParams,
                                  "Editor info action: theme|scale|language|paths|multi_window|distraction_free|toast"),
      {},
      {"{\"action\":\"scale\"}",
       "{\"action\":\"toast\",\"message\":\"Build finished\",\"severity\":\"info\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "editor_ui_action",
      "Act on editor UI elements in one action tool. action='click' clicks the element at path (optional button, double_click, warp, observe); action='type' focuses path and writes text (optional submit, observe); action='shortcut' injects a keyboard shortcut such as 'ctrl+s' or 'f5'. Element paths come from get_editor_ui action=elements or action=hit. Every action modifies editor window state; read the follow-up state with get_editor_state.",
      "Editor", {"editor", "ui", "click", "type", "shortcut", "click_editor_element", "type_editor_element_text", "run_editor_shortcut"}, SideEffect::ModifiesWindow,
      tool_flags::kMutating | tool_flags::kObserve | tool_flags::kCaptureImage,
      kEditorUiActionParams, editor_ui_actions::handle_editor_ui_action,
      schema::build_action_schema(kEditorUiActionActions,
                                  kEditorUiActionParams,
                                  "UI action to run: click|type|shortcut"),
      {
          ToolAlias{"click_editor_element", "editor_ui_action",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "click");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"type_editor_element_text", "editor_ui_action",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "type");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"run_editor_shortcut", "editor_ui_action",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "shortcut");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"click\",\"path\":\"root/Editor/.../Save\"}",
       "{\"action\":\"shortcut\",\"shortcut\":\"ctrl+s\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "editor_config",
      "Change project-level editor configuration in one action tool. action='main_scene' sets application/run/main_scene to path and saves project.godot; action='plugin_enable' enables or disables the named plugin through EditorInterface.set_plugin_enabled. Both write project configuration; the setting takes effect immediately.",
      "Editor", {"editor", "config", "main_scene", "plugin", "enable", "set_editor_main_scene", "set_editor_plugin_enabled"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kEditorConfigParams, editor_ops::handle_editor_config,
      schema::build_action_schema(kEditorConfigActions, kEditorConfigParams,
                                  "Configuration action: main_scene|plugin_enable"),
      {
          ToolAlias{"set_editor_main_scene", "editor_config",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "main_scene");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"set_editor_plugin_enabled", "editor_config",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return editor_alias_action(args, "plugin_enable");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"main_scene\",\"path\":\"res://game.tscn\"}",
       "{\"action\":\"plugin_enable\",\"plugin\":\"godot_autopilot\",\"enabled\":true}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "build_csharp_assembly",
      "Trigger a C# project build with dotnet build --nologo on the res:// root .csproj/.sln. The engine path is globalized to an OS absolute path (ProjectSettings.globalize_path) before it reaches dotnet, because dotnet cannot resolve res:// paths. Intended for CI/command-line compile verification only: it does NOT rewind to an in-editor Build and does not hot-reload the loaded .NET assembly. By default (wait omitted or false) it launches via OS.create_process and returns immediately with result {project_file,project_path,command,pid,started,note}. With wait=true it runs synchronously via OS.execute, temporarily pinning DOTNET_CLI_UI_LANGUAGE=en for the child process and restoring the previous value afterwards so that output_digest carries ASCII English text instead of ACP-mangled localized output, and waits for the BuildResult {project_file,project_path,command,exit_code,success,errors,output_digest,output_empty}: project_file is the res:// project file, project_path the OS absolute path handed to dotnet, command the logical dotnet invocation on the res:// file, exit_code the dotnet exit code, success exit_code==0, errors lists output lines containing error (capped at 50 with errors_truncated), output_digest the combined stdout/stderr truncated to 4000 chars when desensitized else 64000 chars with output_truncated when cut, output_empty true when nothing was captured (output_hint then explains the exit code); timeout_ms declares the wait budget (1-25000, default 25000) validated to stay below the dispatch default 27000ms; errors when the project is not a C# project or dotnet is not available. Both shapes also return next_action (array of executable follow-up steps), restart_required (true when the build succeeded or was launched, independent of whether a game is running, because nothing hot-reloads the loaded assemblies) and game_running. With probe_script the response additionally carries editor_assembly_probe {path,is_dotnet_editor,state,can_instantiate,base_class,hint}: the editor loads the .cs resource without instantiating it, state is 'available' when it loads and 'unavailable' with unavailable=true when the editor is not a .NET build or the load fails; can_instantiate=false on an available probe means the editor still runs the old assembly, so click Build or restart the editor.",
      "Editor", {"build", "csharp", "dotnet"}, SideEffect::Process, tool_flags::kMutating,
      kBuildCsharpAssemblyParams, editor_ops::handle_build_csharp_assembly}));
  v.push_back(make_spec_tool(ToolSpec{
      "dotnet_environment",
      "Probe the .NET/C# environment of the current project without building anything. Use it before build_csharp_assembly or any C# workflow to learn whether the project is a C# project, whether the dotnet CLI is usable and which SDK version it reports, whether the editor itself is a .NET build, and whether the C# runtime looks loaded. Locates the res:// root .csproj/.sln exactly like build_csharp_assembly, runs 'dotnet --version' synchronously to capture the version string, and decides is_dotnet_editor from the CSharpScript class registration (registered when the .NET module loads) with a version-aware fallback to OS.has_feature(dotnet) on 4.4+ or OS.has_feature(mono) before 4.4 — Godot 4.x registers no mono/dotnet feature tag, so both probes report false. Returns result with dotnet_available, dotnet_version, project_file, is_dotnet_editor, assembly_loaded, engine_version, mono_feature, dotnet_feature and hint; errors when the project is not a C# project or dotnet is not available. On a missing project the error carries a ready-to-write template {path, content, target_framework, assembly_name} derived from the engine version and the installed .NET SDK.",
      "Editor", {"dotnet", "csharp", "environment"}, SideEffect::Process, tool_flags::kNone,
      kDotnetEnvironmentParams, editor_ops::handle_dotnet_environment}));
  return v;
}

} // namespace editor_tools
} // namespace godot_autopilot

#endif
