#ifndef GODOT_AUTOPILOT_EDITOR_TOOLS_HPP
#define GODOT_AUTOPILOT_EDITOR_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/editor_ops.hpp"
#include "tools/editor_ui_actions.hpp"
#include "tools/editor_ui_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace editor_tools {

namespace {

const std::vector<ParamSpec> kGetEditorSelectionParams = {};

const std::vector<ParamSpec> kSetEditorSelectionParams = {
    {"paths", "array", "Array of node paths in the edited scene to select, e.g. ['Player', 'Level1/Enemy'] or absolute like '/root/Level1/Enemy'; the previous selection is cleared first and unresolvable paths are skipped", true},
};

const std::vector<ParamSpec> kGetEditorEditedSceneRootParams = {};

const std::vector<ParamSpec> kSaveEditorSceneParams = {
    {"include_hash", "boolean", "Attach FNV-1a content hashes of the memory tree and the on-disk file (memory_hash/disk_hash); off by default, enable for byte-level save proof", false},
    {"include_paths", "boolean", "Always attach the missing_paths list even when node counts match (default: attached only on mismatch)", false},
};

const std::vector<ParamSpec> kSaveEditorScenesParams = {};

const std::vector<ParamSpec> kReloadEditorSceneParams = {
    {"scene_path", "string", "Scene file path to reload from disk, e.g. 'res://game.tscn'; when omitted the currently edited scene is reloaded. All unsaved changes are discarded without confirmation", false},
};

const std::vector<ParamSpec> kInspectEditorResourceParams = {
    {"resource_path", "string", "Path to a resource file to open in the editor's Inspector panel, e.g. 'res://player.tres'; the file is loaded via ResourceLoader and errors when missing or unloadable", true},
};

const std::vector<ParamSpec> kCreateEditorUndoRedoActionParams = {
    {"name", "string", "Human-readable name shown in the editor's undo history, e.g. 'Move player'; group related changes under one name so they revert together", true},
};

const std::vector<ParamSpec> kCommitEditorUndoRedoParams = {};

const std::vector<ParamSpec> kAddEditorUndoRedoDoParams = {
    {"node_path", "string", "Path to the node whose method will be called on redo, e.g. 'Player' or '/root/Player'; resolved against the edited scene root", true},
    {"method", "string", "Name of the node method to call, e.g. 'set_position' or 'queue_free'", true},
    {"value", "object", "Optional serialized JSON value passed as the single method argument; omit for methods without arguments", false},
};

const std::vector<ParamSpec> kAddEditorUndoRedoUndoParams = {
    {"node_path", "string", "Path to the node whose method will be called on undo, e.g. 'Player' or '/root/Player'; resolved against the edited scene root", true},
    {"method", "string", "Name of the node method to call for the inverse operation, e.g. 'set_position'", true},
    {"value", "object", "Optional serialized JSON value passed as the single method argument; omit for methods without arguments", false},
};

const std::vector<ParamSpec> kGetEditorFileSystemTreeParams = {
    {"path", "string", "Directory path to root the tree at, e.g. 'res://scenes' or 'res://'; when omitted the whole project tree is returned. An invalid path returns null", false},
    {"depth", "integer", "Maximum depth below the requested root (default: 12); deeper levels are pruned and the result reports truncated with max_depth", false},
    {"prefix", "string", "Subtree root filter, e.g. 'res://scenes'; takes precedence over path when both are given", false},
    {"filter", "string", "Case-insensitive substring filter matched against each entry's name or path; non-matching leaves are pruned while ancestors of matches are kept, and the result reports filtered", false},
};

const std::vector<ParamSpec> kScanEditorFileSystemParams = {};

const std::vector<ParamSpec> kSetEditorMainSceneParams = {
    {"path", "string", "Path of the scene to launch when the project runs, e.g. 'res://game.tscn'; persisted to project.godot", true},
};

const std::vector<ParamSpec> kPlayEditorCurrentSceneParams = {};

const std::vector<ParamSpec> kStopEditorPlayingParams = {};

const std::vector<ParamSpec> kGetEditorFileSystemStatusParams = {};

const std::vector<ParamSpec> kSetEditorPluginEnabledParams = {
    {"plugin", "string", "Name of the editor plugin to enable or disable, e.g. 'godot_autopilot'", true},
    {"enabled", "boolean", "true activates the plugin, false deactivates it", true},
};

const std::vector<ParamSpec> kCreateEditorSceneParams = {
    {"type", "string", "Godot class name of the root node to create, e.g. 'Node2D' or 'Node3D'; must be a Node subclass, defaults to 'Node'", false},
    {"name", "string", "Name of the new root node, defaults to 'NewRoot'", false},
    {"close_current", "boolean", "Close the current scene tab first if it has no unsaved changes (default: false; errors if the current scene is unsaved). Only the current tab is closed: with multiple scene tabs open, repeat close_editor_scene until no scene is open, otherwise create_editor_scene fails fast without entering the switch wait", false},
    {"timeout_ms", "integer", "Maximum time in milliseconds to wait for the editor to observe the new scene root after add_root_node (default: 2000, min: 50, max: 30000); on timeout the call errors with waited_ms, timeout_ms, node_released and editor_state diagnostics", false},
};

const std::vector<ParamSpec> kOpenEditorSceneParams = {
    {"path", "string", "Path to the scene file to open, e.g. 'res://game.tscn'; errors when the current scene has unsaved changes", true},
};

const std::vector<ParamSpec> kSaveEditorSceneAsParams = {
    {"path", "string", "Target file path for the save, e.g. 'res://levels/level1.tscn'; missing parent directories are created automatically", true},
    {"include_hash", "boolean", "Attach FNV-1a content hashes of the memory tree and the on-disk file (memory_hash/disk_hash); off by default, enable for byte-level save proof", false},
    {"include_paths", "boolean", "Always attach the missing_paths list even when node counts match (default: attached only on mismatch)", false},
};

const std::vector<ParamSpec> kVerifySceneSavedParams = {
    {"scene_path", "string", "Scene file path to compare against the in-memory tree, e.g. 'res://game.tscn'; when omitted the current scene file path is used", false},
    {"include_hash", "boolean", "Attach FNV-1a content hashes of the memory tree and the on-disk file (memory_hash/disk_hash); off by default", false},
};

const std::vector<ParamSpec> kBuildCsharpAssemblyParams = {};

const std::vector<ParamSpec> kCloseEditorSceneParams = {};

const std::vector<ParamSpec> kGetEditorViewportGeometryParams = {
    {"viewport", "string", "Viewport to report: '2d' (default) or '3d'", false},
    {"index", "integer", "3D viewport index, 0-3 (default: 0); ignored when viewport='2d'", false},
};

const std::vector<ParamSpec> kGetEditorUiElementsParams = {
    {"query", "string", "Case-insensitive substring filter matched against path, name, text, tooltip and placeholder", false},
    {"type_filter", "string", "Exact class name to restrict results to, e.g. 'Button' or 'LineEdit'", false},
    {"interactive_only", "boolean", "Keep only actionable controls such as buttons, line edits, trees and sliders (default: false)", false},
    {"max_elements", "integer", "Maximum number of elements to return, an integer from 1 to 1000 (default: 100); the result reports truncated when the cap is hit", false},
};

const std::vector<ParamSpec> kHitTestEditorPointParams = {
    {"position", "object", "Point to test, object with numeric x and y fields, in pixels relative to the client area of window_id", true},
    {"window_id", "integer", "Window to test against (default: 0 = main editor window), e.g. an id from get_editor_ui_elements", false},
    {"max_results", "integer", "Maximum number of hit controls to return, an integer from 1 to 64 (default: 10)", false},
    {"include_scene_nodes", "boolean", "Append scene context (default: false): scene_tree_item, the scene-tree row under the point when a row is hit, and scene_nodes, the topmost-first scene node paths with name/type under the point inside the 2D editor viewport, plus scene_node_count; scene_tree_error/scene_nodes_error report unavailable context", false},
};

const std::vector<ParamSpec> kSceneTreeItemsParams = {
    {"filter", "string", "Case-insensitive substring filter matched against each row's scene-relative path or name", false},
    {"selected_only", "boolean", "Keep only rows currently selected in the scene tree (default: false)", false},
    {"max_items", "integer", "Maximum number of rows to return, an integer from 1 to 1000 (default: 200); the result reports truncated when the cap is hit", false},
};

const std::vector<ParamSpec> kSelectSceneTreeNodeParams = {
    {"path", "string", "Node path in the edited scene: scene-relative such as 'Player' or 'Level1/Enemy', or absolute such as '/root/Level1/Enemy'; the previous selection is cleared first unless add is true", true},
    {"add", "boolean", "true appends the node to the current selection instead of replacing it (default: false)", false},
    {"inspect", "boolean", "Open the node in the Inspector through EditorInterface.edit_node (default: true)", false},
    {"focus", "boolean", "Scroll the scene tree to the node row when the row is found (default: true)", false},
};

const std::vector<ParamSpec> kClickEditorElementParams = {
    {"path", "string", "Element path as returned by get_editor_ui_elements or hit_test_editor_point", true},
    {"button", "string", "Mouse button to click: left, right or middle (default: left)", false},
    {"double_click", "boolean", "Send a second press/release pair carrying the double-click flag (default: false)", false},
    {"warp", "boolean", "Warp the physical cursor onto the element center before clicking (default: true)", false},
    {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
};

const std::vector<ParamSpec> kTypeEditorElementTextParams = {
    {"path", "string", "Element path as returned by get_editor_ui_elements or hit_test_editor_point", true},
    {"text", "string", "Text to write into the focused element; may contain any UTF-8 text", true},
    {"submit", "boolean", "Append an Enter key press and release after the text (default: false)", false},
    {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
};

const std::vector<ParamSpec> kRunEditorShortcutParams = {
    {"shortcut", "string", "Shortcut string with '+'-separated modifiers (ctrl/control, shift, alt, meta/super) followed by the main key, e.g. 'ctrl+s', 'ctrl+shift+z' or 'f5'; the key accepts the input tool key names plus F1-F12", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(32);
  v.push_back(make_spec_tool(ToolSpec{
      "get_editor_selection",
      "Retrieve the list of nodes currently selected in the edited scene. Use it to see which nodes the user has selected before operating on them with scene tools. Returns result as an array of objects, each with name, class and path fields relative to the scene root; an empty array means nothing is selected.",
      "Editor", {"editor", "selection", "get"}, SideEffect::None, tool_flags::kNone,
      kGetEditorSelectionParams, editor_ops::handle_get_selection}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_editor_selection",
      "Set the node selection in the edited scene to exactly the given paths. Use it to focus editor operations on specific nodes, e.g. before inspecting or modifying them; the previous selection is cleared first. Accepts an array of paths, and paths that cannot be resolved are skipped silently. Returns result 'ok'.",
      "Editor", {"editor", "selection", "set"}, SideEffect::None, tool_flags::kNone,
      kSetEditorSelectionParams, editor_ops::handle_set_selection}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_editor_edited_scene_root",
      "Fetch the root node of the currently edited scene in the editor. Use it at the start of a scene editing workflow to learn the root's name, class and path; returns result with name, type and path fields. Returns null when no scene is open. Combine with get_scene_tree for full hierarchy information.",
      "Editor", {"editor", "scene", "root"}, SideEffect::None, tool_flags::kNone,
      kGetEditorEditedSceneRootParams, editor_ops::handle_get_edited_scene_root}));
  v.push_back(make_spec_tool(ToolSpec{
      "save_editor_scene",
      "Save the currently edited scene to disk. Use it before playing or closing a scene so recent changes are not lost. Returns result 'saved' plus the path written to. A scene that was never saved is automatically written to res://<root node name>.tscn and the note field explains the fallback; use save_editor_scene_as to choose an explicit path instead.",
      "Editor", {"editor", "scene", "save"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSaveEditorSceneParams, editor_ops::handle_save_scene}));
  v.push_back(make_spec_tool(ToolSpec{
      "save_editor_scenes",
      "Save every open scene tab in the editor to disk at once. Use it when several scenes were edited and you want to persist them all in one call, in contrast to save_editor_scene which saves only the currently edited scene. Returns result 'ok'; scenes without a file path may not be written to disk.",
      "Editor", {"editor", "scene", "save_all"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSaveEditorScenesParams, editor_ops::handle_save_all_scenes}));
  v.push_back(make_spec_tool(ToolSpec{
      "reload_editor_scene",
      "Reload a scene from disk, replacing its in-memory state; reloads the current edited scene unless scene_path is provided. Use it to discard experimental edits and restore the saved version. Note that all unsaved changes are dropped without any confirmation, so call save_editor_scene first if the changes matter. Returns result 'ok'.",
      "Editor", {"editor", "scene", "reload"}, SideEffect::None, tool_flags::kNone,
      kReloadEditorSceneParams, editor_ops::handle_reload_scene}));
  v.push_back(make_spec_tool(ToolSpec{
      "inspect_editor_resource",
      "Open a resource file in the editor's Inspector panel for inspection. Use it after locating a resource, e.g. a .tres or .tscn file, to review and edit its properties visually. Takes resource_path and loads the file through ResourceLoader; errors when the path is missing or the file fails to load. Returns result 'ok'.",
      "Editor", {"editor", "inspect"}, SideEffect::None, tool_flags::kNone,
      kInspectEditorResourceParams, editor_ops::handle_inspect_object}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_editor_undo_redo_action",
      "Start a new undo/redo action in the editor's undo history with the given name. Use it to group several operations so a single undo step reverts them all. Must be followed by add_editor_undo_redo_do or add_editor_undo_redo_undo calls and finally commit_editor_undo_redo. Returns result 'ok'.",
      "Editor", {"editor", "undo", "start"}, SideEffect::None, tool_flags::kUndoable,
      kCreateEditorUndoRedoActionParams, editor_ops::handle_undo_redo_start}));
  v.push_back(make_spec_tool(ToolSpec{
      "commit_editor_undo_redo",
      "Commit the currently open undo/redo action so it appears in the editor's undo history as a single step. Use it to finalize a sequence started with create_editor_undo_redo_action and filled with add_editor_undo_redo_do or add_editor_undo_redo_undo calls; after committing, the action can no longer be extended. Returns result 'ok'.",
      "Editor", {"editor", "undo", "commit"}, SideEffect::None, tool_flags::kNone,
      kCommitEditorUndoRedoParams, editor_ops::handle_undo_redo_commit}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_editor_undo_redo_do",
      "Register a method call that runs when the undo/redo action is applied (the do step). Use it inside an action started with create_editor_undo_redo_action; target a node by node_path, name the method and pass an optional value argument. Pair it with add_editor_undo_redo_undo for the inverse, then finish with commit_editor_undo_redo. Returns result 'ok'.",
      "Editor", {"editor", "undo", "do"}, SideEffect::None, tool_flags::kNone,
      kAddEditorUndoRedoDoParams, editor_ops::handle_undo_redo_add_do}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_editor_undo_redo_undo",
      "Register a method call that runs when the undo/redo action is reverted (the undo step). Use it to define the inverse of an operation added with add_editor_undo_redo_do; target a node by node_path, name the method and pass an optional value argument. Finish the action with commit_editor_undo_redo. Returns result 'ok'.",
      "Editor", {"editor", "undo", "undo"}, SideEffect::None, tool_flags::kNone,
      kAddEditorUndoRedoUndoParams, editor_ops::handle_undo_redo_add_undo}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_editor_file_system_tree",
      "Fetch the editor's file system directory tree, optionally rooted at the given path. Use it to discover project structure and resource paths before loading files. Directory entries contain name, path, type and children; file entries carry name, path and type. Optional 'depth' caps levels below the root (default 12, deeper levels pruned with truncated and max_depth reported), 'prefix' narrows the tree to a subtree root (takes precedence over path) and 'filter' keeps only entries whose name or path contains the given substring case-insensitively (ancestors of matches kept, filtered reported); an invalid path returns null.",
      "Editor", {"editor", "filesystem", "resources"}, SideEffect::None, tool_flags::kNone,
      kGetEditorFileSystemTreeParams, editor_ops::handle_file_system_get_resources}));
  v.push_back(make_spec_tool(ToolSpec{
      "scan_editor_file_system",
      "Trigger a full rescan of the project file system so new, changed or deleted files are picked up by the editor. Use it after creating or modifying files outside the editor, or after saving a scene to a new path. The scan runs asynchronously: returns result 'ok' immediately, then poll get_editor_file_system_status until scanning is false.",
      "Editor", {"editor", "filesystem", "scan"}, SideEffect::None, tool_flags::kNone,
      kScanEditorFileSystemParams, editor_ops::handle_file_system_scan}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_editor_main_scene",
      "Set the project's main scene, the scene launched when the project runs. Use it to change which scene starts first, e.g. after creating a new level with create_editor_scene. Takes the scene path such as res://game.tscn and persists the setting to project.godot. Returns result 'ok'.",
      "Editor", {"editor", "scene", "main"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kSetEditorMainSceneParams, editor_ops::handle_set_main_scene}));
  v.push_back(make_spec_tool(ToolSpec{
      "play_editor_current_scene",
      "Launch the currently edited scene as a game in the editor. Use it to start testing gameplay; the scene should be saved first or playback may fail. Returns result 'ok' or 'already_playing' plus the is_playing flag. While the game runs, use Game and Debugger tools (get_game_status, capture_game_viewport, execute_game_script) and stop playback with stop_editor_playing.",
      "Editor", {"editor", "play", "scene"}, SideEffect::None, tool_flags::kNone,
      kPlayEditorCurrentSceneParams, editor_ops::handle_play_current_scene}));
  v.push_back(make_spec_tool(ToolSpec{
      "stop_editor_playing",
      "Stop the running game process started by play_editor_current_scene and return the editor to editing mode. Use it at the end of a play-testing session or before modifying the scene again. Returns result 'ok' even when no game is currently running.",
      "Editor", {"editor", "stop", "playing"}, SideEffect::None, tool_flags::kNone,
      kStopEditorPlayingParams, editor_ops::handle_stop_playing}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_editor_file_system_status",
      "Report the current state of the editor file system scan. Use it after scan_editor_file_system to find out when the scan finishes. Returns result with scanning (boolean) and progress (float between 0.0 and 1.0). Wait until scanning is false before relying on file system queries such as get_editor_file_system_tree.",
      "Editor", {"editor", "filesystem", "get"}, SideEffect::None, tool_flags::kNone,
      kGetEditorFileSystemStatusParams, editor_ops::handle_get_resource_filesystem}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_editor_plugin_enabled",
      "Enable or disable an editor plugin by its plugin name. Use it to activate plugins whose tools should be available, e.g. 'godot_autopilot', or to disable one at runtime. Takes the plugin name and an enabled boolean; the setting takes effect immediately. Returns result 'ok'.",
      "Editor", {"editor", "plugin", "enable"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kSetEditorPluginEnabledParams, editor_ops::handle_set_plugin_enabled}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_editor_scene",
      "Create a new empty scene with a root node of the given type in the editor. Use it to start building a new level; type defaults to Node, name defaults to NewRoot, and close_current (default false) closes the current scene tab automatically when it has no unsaved changes. Only the current tab is closed: with multiple scene tabs open, repeat close_editor_scene until no scene is open, otherwise this call fails fast with the neighboring tab reported in editor_state instead of waiting. Returns result with path, type and closed_previous; errors when the current scene is unsaved or type is not a Node subclass.",
      "Editor", {"editor", "scene", "new"}, SideEffect::None, tool_flags::kNone,
      kCreateEditorSceneParams, editor_ops::handle_new_scene}));
  v.push_back(make_spec_tool(ToolSpec{
      "open_editor_scene",
      "Open an existing scene file, e.g. res://game.tscn, in the editor, replacing the currently edited scene. Use it to switch between scenes during editing, e.g. to continue work on another level. Errors when the current scene has unsaved changes, so call save_editor_scene first. Returns result 'ok'.",
      "Editor", {"scene", "open"}, SideEffect::None, tool_flags::kNone,
      kOpenEditorSceneParams, editor_ops::handle_open_scene}));
  v.push_back(make_spec_tool(ToolSpec{
      "save_editor_scene_as",
      "Save the currently edited scene to a specific file path, creating missing parent directories automatically. Use it when the scene has no file path yet or when you want to store a copy at another location. Errors when the file is not created after saving. Returns result 'saved'.",
      "Editor", {"editor", "scene", "save"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSaveEditorSceneAsParams, editor_ops::handle_save_scene_as}));
  v.push_back(make_spec_tool(ToolSpec{
      "verify_scene_saved",
      "Compare the in-memory edited scene tree against its on-disk .tscn file without modifying anything. Use it after save_editor_scene or save_editor_scene_as to prove the save persisted every node (e.g. after moving a subtree with children), or as a standalone consistency check. Returns result 'match' or 'mismatch' plus memory_nodes, disk_nodes, match, missing_paths (memory paths absent on disk, capped at 50 with missing_truncated) and path; optional scene_path overrides the compared file, include_hash adds FNV-1a memory_hash/disk_hash. Read-only.",
      "Editor", {"editor", "scene", "verify", "save"}, SideEffect::None, tool_flags::kNone,
      kVerifySceneSavedParams, editor_ops::handle_verify_scene_saved}));
  v.push_back(make_spec_tool(ToolSpec{
      "build_csharp_assembly",
      "Trigger a C# project build by launching 'dotnet build --nologo <project>' as an async OS process on the res:// root .csproj/.sln. Intended for CI/command-line compile verification only: it does NOT rewind to an in-editor Build and does not hot-reload the loaded .NET assembly. Returns result {'project_file','command','started','pid','note'}; errors when the project is not a C# project or dotnet is not available.",
      "Editor", {"build", "csharp", "dotnet"}, SideEffect::Process, tool_flags::kMutating,
      kBuildCsharpAssemblyParams, editor_ops::handle_build_csharp_assembly}));
  v.push_back(make_spec_tool(ToolSpec{
      "close_editor_scene",
      "Close the currently edited scene tab and return the editor to an empty state when it was the last tab. Use it before opening or creating another scene, or when a scene is no longer needed. Only the current tab is closed: with multiple scene tabs open, call it repeatedly until no scene is open. Refuses to close when the scene has unsaved changes, so call save_editor_scene first. Returns result 'closed'.",
      "Editor", {"editor", "close", "scene"}, SideEffect::None, tool_flags::kNone,
      kCloseEditorSceneParams, editor_ops::handle_close_scene}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_editor_viewport_geometry",
      "Report how the editor viewport image maps onto the window client area as window = offset + image * scale. Use it to convert pixels of a capture_editor_viewport image into client-area coordinates for the input tools, or to project client-area points back into the 2D canvas. Returns result with viewport, window_id, image_width/image_height, viewport_size, mapping (scale_x, scale_y, offset_x, offset_y) and, for the 2D viewport, the canvas origin and zoom. Optional 'viewport' picks '2d' (default) or '3d', and 'index' selects the 3D viewport (0-3).",
      "Editor", {"editor", "viewport", "geometry"}, SideEffect::None, tool_flags::kNone,
      kGetEditorViewportGeometryParams, editor_ui_ops::handle_get_editor_viewport_geometry}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_editor_ui_elements",
      "Enumerate the editor's UI controls (buttons, inputs, lists and other visible panels) with their semantic path, type, text, tooltip, client-area rectangle and window_id. Use it as the semantic-first path for editor automation: pick a target from the listing, then act on it with click_editor_element or type_editor_element_text instead of guessing raw coordinates. Optional 'query' matches path/name/text/tooltip/placeholder substrings, 'type_filter' restricts to an exact class name, 'interactive_only' keeps actionable controls such as buttons and line edits, and 'max_elements' caps the listing (1-1000, default 100). Returns result with elements, count and truncated.",
      "Editor", {"editor", "ui", "elements"}, SideEffect::None, tool_flags::kNone,
      kGetEditorUiElementsParams, editor_ui_ops::handle_get_editor_ui_elements}));
  v.push_back(make_spec_tool(ToolSpec{
      "hit_test_editor_point",
      "Return the chain of editor UI controls under a client-area point, topmost control first. Use it to preview what click_editor_element would hit at a location or to identify a control from a screenshot coordinate; the hit rule approximates the engine's own rule (clipping, mouse_filter, visibility). Takes position (numeric x and y in the client area of window_id, default 0 = main editor window) plus optional max_results (1-64, default 10). Optional 'include_scene_nodes' (default false) appends scene context: scene_tree_item with the scene-tree row under the point when one is hit, and scene_nodes, the topmost-first scene node paths (Node2D/Control with a known rect) under the point inside the 2D editor viewport, with scene_node_count, plus scene_tree_error/scene_nodes_error when that context is unavailable. Returns result with hits, count and space.",
      "Editor", {"editor", "hit", "test"}, SideEffect::None, tool_flags::kNone,
      kHitTestEditorPointParams, editor_ui_ops::handle_hit_test_editor_point}));
  v.push_back(make_spec_tool(ToolSpec{
      "scene_tree_items",
      "Enumerate the rows of the editor's Scene dock tree, i.e. the scene nodes as the editor displays them. Use it to locate a scene node row before selecting or clicking it, because get_editor_ui_elements only lists Control nodes and cannot see Tree rows. Each row carries path (scene-relative, e.g. 'Player'), name, type, depth, selected, collapsed, visible and rect in window/client-area pixels (rect is null for rows hidden by a collapsed ancestor). The scene tree is located by finding the Tree control whose row metadata resolves to a node of the edited scene. Optional 'filter' matches path or name substrings case-insensitively, 'selected_only' keeps only rows selected in the tree and 'max_items' caps the listing (1-1000, default 200) with a truncated flag. Returns result with rows, count, truncated, window_id and space; errors when no scene is open or the scene tree control is unavailable, in which case use get_scene_tree for the scene model.",
      "Editor", {"editor", "scene_tree", "items", "list"}, SideEffect::None, tool_flags::kNone,
      kSceneTreeItemsParams, editor_ui_ops::handle_scene_tree_items}));
  v.push_back(make_spec_tool(ToolSpec{
      "select_scene_tree_node",
      "Select a node in the edited scene tree and show it in the Inspector, mirroring a user click in the Scene dock. Provide 'path' (scene-relative such as 'Player', or absolute such as '/root/Level1/Enemy'); the previous selection is cleared first unless 'add' is true, which appends to it. 'inspect' (default true) opens the node in the Inspector via EditorInterface.edit_node; 'focus' (default true) scrolls the scene tree to the row when the row is found. Selection and inspector still apply when the scene tree control is unavailable (tree_found=false and focused=false in the response). Returns result with ok, path, selected_count, inspected, focused and tree_found; errors when the node does not exist or no scene is open.",
      "Editor", {"editor", "scene_tree", "select", "node"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kSelectSceneTreeNodeParams, editor_ui_ops::handle_select_scene_tree_node}));
  v.push_back(make_spec_tool(ToolSpec{
      "click_editor_element",
      "Click an editor UI element resolved by its path, injecting warp, motion, press and release in one call; double_click adds a second press/release pair carrying the double-click flag. Element paths come from get_editor_ui_elements or hit_test_editor_point, so prefer this over raw-coordinate clicks. Optional 'button' accepts left, right or middle (default left) and 'warp' (default true) moves the physical cursor onto the element first; disable it for controls that react to hover. Optional 'observe' (default false) appends a fresh editor viewport capture to the result. Returns result with ok, path, type, button, double_click, window_id and position. Editor process only.",
      "Editor", {"editor", "ui", "click"}, SideEffect::ModifiesWindow,
      tool_flags::kMutating | tool_flags::kObserve | tool_flags::kCaptureImage, kClickEditorElementParams,
      editor_ui_actions::handle_click_editor_element}));
  v.push_back(make_spec_tool(ToolSpec{
      "type_editor_element_text",
      "Focus an editor UI element and write text into it by pushing the whole string as one text-input event instead of simulating keystrokes. Use it to fill editor fields such as search boxes or property inputs resolved through get_editor_ui_elements; the element is focused with grab_focus when it does not already have focus. Optional 'submit' (default false) appends an Enter press and release. Optional 'observe' (default false) appends a fresh editor viewport capture to the result. Returns result with ok, path, length, focused and submit. Editor process only.",
      "Editor", {"editor", "ui", "type"}, SideEffect::ModifiesWindow,
      tool_flags::kMutating | tool_flags::kObserve | tool_flags::kCaptureImage,
      kTypeEditorElementTextParams,
      editor_ui_actions::handle_type_editor_element_text}));
  v.push_back(make_spec_tool(ToolSpec{
      "run_editor_shortcut",
      "Inject an editor keyboard shortcut to run a command, e.g. 'ctrl+s' to save, 'ctrl+shift+z' to redo or 'f5' to play the project. The string is parsed as '+'-separated modifiers (ctrl/control, shift, alt, meta/super, case-insensitive) followed by the main key, which accepts the same names as the input tools plus F1-F12. The press and release are injected with the modifier flags set; a malformed shortcut or an unknown key returns an error. Returns result with ok, shortcut, key and the ctrl/shift/alt/meta flags. Editor process only.",
      "Editor", {"editor", "shortcut", "input"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kRunEditorShortcutParams, editor_ui_actions::handle_run_editor_shortcut}));
  return v;
}

} // namespace editor_tools
} // namespace godot_autopilot

#endif