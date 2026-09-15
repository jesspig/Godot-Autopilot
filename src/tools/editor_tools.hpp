#ifndef GODOT_AUTOPILOT_EDITOR_TOOLS_HPP
#define GODOT_AUTOPILOT_EDITOR_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/editor_ops.hpp"
#include "tools/editor_ui_actions.hpp"
#include "tools/editor_ui_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace editor_tools {

GDA_TOOL_CLASS(GetEditorSelectionTool, "get_editor_selection",
               "Retrieve the list of nodes currently selected in the edited scene. Use it to see which nodes the user has selected before operating on them with scene tools. Returns result as an array of objects, each with name, class and path fields relative to the scene root; an empty array means nothing is selected.",
               "Editor", std::vector<std::string>({"editor", "selection", "get"}), editor_ops::handle_get_selection, true)

GDA_TOOL_CLASS(SetEditorSelectionTool, "set_editor_selection",
               "Set the node selection in the edited scene to exactly the given paths. Use it to focus editor operations on specific nodes, e.g. before inspecting or modifying them; the previous selection is cleared first. Accepts an array of paths, and paths that cannot be resolved are skipped silently. Returns result 'ok'.",
               "Editor", std::vector<std::string>({"editor", "selection", "set"}), editor_ops::handle_set_selection, true)

GDA_TOOL_CLASS(GetEditorEditedSceneRootTool, "get_editor_edited_scene_root",
               "Fetch the root node of the currently edited scene in the editor. Use it at the start of a scene editing workflow to learn the root's name, class and path; returns result with name, type and path fields. Returns null when no scene is open. Combine with get_scene_tree for full hierarchy information.",
               "Editor", std::vector<std::string>({"editor", "scene", "root"}), editor_ops::handle_get_edited_scene_root, true)

GDA_TOOL_CLASS_SIDE(SaveEditorSceneTool, "save_editor_scene",
               "Save the currently edited scene to disk. Use it before playing or closing a scene so recent changes are not lost. Returns result 'saved' plus the path written to. A scene that was never saved is automatically written to res://<root node name>.tscn and the note field explains the fallback; use save_editor_scene_as to choose an explicit path instead.",
               "Editor", std::vector<std::string>({"editor", "scene", "save"}), editor_ops::handle_save_scene, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS_SIDE(SaveEditorScenesTool, "save_editor_scenes",
               "Save every open scene tab in the editor to disk at once. Use it when several scenes were edited and you want to persist them all in one call, in contrast to save_editor_scene which saves only the currently edited scene. Returns result 'ok'; scenes without a file path may not be written to disk.",
               "Editor", std::vector<std::string>({"editor", "scene", "save_all"}), editor_ops::handle_save_all_scenes, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS(ReloadEditorSceneTool, "reload_editor_scene",
               "Reload a scene from disk, replacing its in-memory state; reloads the current edited scene unless scene_path is provided. Use it to discard experimental edits and restore the saved version. Note that all unsaved changes are dropped without any confirmation, so call save_editor_scene first if the changes matter. Returns result 'ok'.",
               "Editor", std::vector<std::string>({"editor", "scene", "reload"}), editor_ops::handle_reload_scene, true)

GDA_TOOL_CLASS(InspectEditorResourceTool, "inspect_editor_resource",
               "Open a resource file in the editor's Inspector panel for inspection. Use it after locating a resource, e.g. a .tres or .tscn file, to review and edit its properties visually. Takes resource_path and loads the file through ResourceLoader; errors when the path is missing or the file fails to load. Returns result 'ok'.",
               "Editor", std::vector<std::string>({"editor", "inspect"}), editor_ops::handle_inspect_object, true)

GDA_TOOL_CLASS(CreateEditorUndoRedoActionTool, "create_editor_undo_redo_action",
               "Start a new undo/redo action in the editor's undo history with the given name. Use it to group several operations so a single undo step reverts them all. Must be followed by add_editor_undo_redo_do or add_editor_undo_redo_undo calls and finally commit_editor_undo_redo. Returns result 'ok'.",
               "Editor", std::vector<std::string>({"editor", "undo", "start"}), editor_ops::handle_undo_redo_start, true)

GDA_TOOL_CLASS(CommitEditorUndoRedoTool, "commit_editor_undo_redo",
               "Commit the currently open undo/redo action so it appears in the editor's undo history as a single step. Use it to finalize a sequence started with create_editor_undo_redo_action and filled with add_editor_undo_redo_do or add_editor_undo_redo_undo calls; after committing, the action can no longer be extended. Returns result 'ok'.",
               "Editor", std::vector<std::string>({"editor", "undo", "commit"}), editor_ops::handle_undo_redo_commit, true)

GDA_TOOL_CLASS(AddEditorUndoRedoDoTool, "add_editor_undo_redo_do",
               "Register a method call that runs when the undo/redo action is applied (the do step). Use it inside an action started with create_editor_undo_redo_action; target a node by node_path, name the method and pass an optional value argument. Pair it with add_editor_undo_redo_undo for the inverse, then finish with commit_editor_undo_redo. Returns result 'ok'.",
               "Editor", std::vector<std::string>({"editor", "undo", "do"}), editor_ops::handle_undo_redo_add_do, true)

GDA_TOOL_CLASS(AddEditorUndoRedoUndoTool, "add_editor_undo_redo_undo",
               "Register a method call that runs when the undo/redo action is reverted (the undo step). Use it to define the inverse of an operation added with add_editor_undo_redo_do; target a node by node_path, name the method and pass an optional value argument. Finish the action with commit_editor_undo_redo. Returns result 'ok'.",
               "Editor", std::vector<std::string>({"editor", "undo", "undo"}), editor_ops::handle_undo_redo_add_undo, true)

GDA_TOOL_CLASS(GetEditorFileSystemTreeTool, "get_editor_file_system_tree",
               "Fetch the editor's file system directory tree, optionally rooted at the given path. Use it to discover project structure and resource paths before loading files. Directory entries contain name, path, type and children; file entries carry name, path and type. Trees deeper than 12 levels are truncated and a max_depth field is returned; an invalid path returns null.",
               "Editor", std::vector<std::string>({"editor", "filesystem", "resources"}), editor_ops::handle_file_system_get_resources, true)

GDA_TOOL_CLASS(ScanEditorFileSystemTool, "scan_editor_file_system",
               "Trigger a full rescan of the project file system so new, changed or deleted files are picked up by the editor. Use it after creating or modifying files outside the editor, or after saving a scene to a new path. The scan runs asynchronously: returns result 'ok' immediately, then poll get_editor_file_system_status until scanning is false.",
               "Editor", std::vector<std::string>({"editor", "filesystem", "scan"}), editor_ops::handle_file_system_scan, true)

GDA_TOOL_CLASS_SIDE(SetEditorMainSceneTool, "set_editor_main_scene",
               "Set the project's main scene, the scene launched when the project runs. Use it to change which scene starts first, e.g. after creating a new level with create_editor_scene. Takes the scene path such as res://game.tscn and persists the setting to project.godot. Returns result 'ok'.",
               "Editor", std::vector<std::string>({"editor", "scene", "main"}), editor_ops::handle_set_main_scene, true, ::godot_autopilot::SideEffect::WritesConfig)

GDA_TOOL_CLASS(PlayEditorCurrentSceneTool, "play_editor_current_scene",
               "Launch the currently edited scene as a game in the editor. Use it to start testing gameplay; the scene should be saved first or playback may fail. Returns result 'ok' or 'already_playing' plus the is_playing flag. While the game runs, use Game and Debugger tools (get_game_status, capture_game_viewport, execute_game_script) and stop playback with stop_editor_playing.",
               "Editor", std::vector<std::string>({"editor", "play", "scene"}), editor_ops::handle_play_current_scene, true)

GDA_TOOL_CLASS(StopEditorPlayingTool, "stop_editor_playing",
               "Stop the running game process started by play_editor_current_scene and return the editor to editing mode. Use it at the end of a play-testing session or before modifying the scene again. Returns result 'ok' even when no game is currently running.",
               "Editor", std::vector<std::string>({"editor", "stop", "playing"}), editor_ops::handle_stop_playing, true)

GDA_TOOL_CLASS(GetEditorFileSystemStatusTool, "get_editor_file_system_status",
               "Report the current state of the editor file system scan. Use it after scan_editor_file_system to find out when the scan finishes. Returns result with scanning (boolean) and progress (float between 0.0 and 1.0). Wait until scanning is false before relying on file system queries such as get_editor_file_system_tree.",
               "Editor", std::vector<std::string>({"editor", "filesystem", "get"}), editor_ops::handle_get_resource_filesystem, true)

GDA_TOOL_CLASS_SIDE(SetEditorPluginEnabledTool, "set_editor_plugin_enabled",
               "Enable or disable an editor plugin by its plugin name. Use it to activate plugins whose tools should be available, e.g. 'godot_autopilot', or to disable one at runtime. Takes the plugin name and an enabled boolean; the setting takes effect immediately. Returns result 'ok'.",
               "Editor", std::vector<std::string>({"editor", "plugin", "enable"}), editor_ops::handle_set_plugin_enabled, true, ::godot_autopilot::SideEffect::WritesConfig)

GDA_TOOL_CLASS(CreateEditorSceneTool, "create_editor_scene",
               "Create a new empty scene with a root node of the given type in the editor. Use it to start building a new level; type defaults to Node, name defaults to NewRoot, and close_current (default false) closes the previous scene automatically when it has no unsaved changes. Returns result with path, type and closed_previous; errors when the current scene is unsaved or type is not a Node subclass.",
               "Editor", std::vector<std::string>({"editor", "scene", "new"}), editor_ops::handle_new_scene, true)

GDA_TOOL_CLASS(OpenEditorSceneTool, "open_editor_scene",
               "Open an existing scene file, e.g. res://game.tscn, in the editor, replacing the currently edited scene. Use it to switch between scenes during editing, e.g. to continue work on another level. Errors when the current scene has unsaved changes, so call save_editor_scene first. Returns result 'ok'.",
               "Editor", std::vector<std::string>({"scene", "open"}), editor_ops::handle_open_scene, true)

GDA_TOOL_CLASS_SIDE(SaveEditorSceneAsTool, "save_editor_scene_as",
               "Save the currently edited scene to a specific file path, creating missing parent directories automatically. Use it when the scene has no file path yet or when you want to store a copy at another location. Errors when the file is not created after saving. Returns result 'saved'.",
               "Editor", std::vector<std::string>({"editor", "scene", "save"}), editor_ops::handle_save_scene_as, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS_SIDE(BuildCsharpAssemblyTool, "build_csharp_assembly",
               "Trigger a C# project build by launching 'dotnet build --nologo <project>' as an async OS process on the res:// root .csproj/.sln. Intended for CI/command-line compile verification only: it does NOT rewind to an in-editor Build and does not hot-reload the loaded .NET assembly. Returns result {'project_file','command','started','pid','note'}; errors when the project is not a C# project or dotnet is not available.",
               "Editor", std::vector<std::string>({"build", "csharp", "dotnet"}), editor_ops::handle_build_csharp_assembly, true, ::godot_autopilot::SideEffect::Process)

GDA_TOOL_CLASS(CloseEditorSceneTool, "close_editor_scene",
               "Close the currently edited scene and return the editor to an empty state. Use it before opening or creating another scene, or when a scene is no longer needed. Refuses to close when the scene has unsaved changes, so call save_editor_scene first. Returns result 'closed'.",
               "Editor", std::vector<std::string>({"editor", "close", "scene"}), editor_ops::handle_close_scene, true)

GDA_TOOL_CLASS(GetEditorViewportGeometryTool, "get_editor_viewport_geometry",
               "Report how the editor viewport image maps onto the window client area as window = offset + image * scale. Use it to convert pixels of a capture_editor_viewport image into client-area coordinates for the input tools, or to project client-area points back into the 2D canvas. Returns result with viewport, window_id, image_width/image_height, viewport_size, mapping (scale_x, scale_y, offset_x, offset_y) and, for the 2D viewport, the canvas origin and zoom. Optional 'viewport' picks '2d' (default) or '3d', and 'index' selects the 3D viewport (0-3).",
               "Editor", std::vector<std::string>({"editor", "viewport", "geometry"}), editor_ui_ops::handle_get_editor_viewport_geometry, true)

GDA_TOOL_CLASS(GetEditorUiElementsTool, "get_editor_ui_elements",
               "Enumerate the editor's UI controls (buttons, inputs, lists and other visible panels) with their semantic path, type, text, tooltip, client-area rectangle and window_id. Use it as the semantic-first path for editor automation: pick a target from the listing, then act on it with click_editor_element or type_editor_element_text instead of guessing raw coordinates. Optional 'query' matches path/name/text/tooltip/placeholder substrings, 'type_filter' restricts to an exact class name, 'interactive_only' keeps actionable controls such as buttons and line edits, and 'max_elements' caps the listing (1-1000, default 100). Returns result with elements, count and truncated.",
               "Editor", std::vector<std::string>({"editor", "ui", "elements"}), editor_ui_ops::handle_get_editor_ui_elements, true)

GDA_TOOL_CLASS(HitTestEditorPointTool, "hit_test_editor_point",
               "Return the chain of editor UI controls under a client-area point, topmost control first. Use it to preview what click_editor_element would hit at a location or to identify a control from a screenshot coordinate; the hit rule approximates the engine's own rule (clipping, mouse_filter, visibility). Takes position (numeric x and y in the client area of window_id, default 0 = main editor window) plus optional max_results (1-64, default 10). Returns result with hits, count and space.",
               "Editor", std::vector<std::string>({"editor", "hit", "test"}), editor_ui_ops::handle_hit_test_editor_point, true)

GDA_TOOL_CLASS_SIDE(ClickEditorElementTool, "click_editor_element",
               "Click an editor UI element resolved by its path, injecting warp, motion, press and release in one call; double_click adds a second press/release pair carrying the double-click flag. Element paths come from get_editor_ui_elements or hit_test_editor_point, so prefer this over raw-coordinate clicks. Optional 'button' accepts left, right or middle (default left) and 'warp' (default true) moves the physical cursor onto the element first; disable it for controls that react to hover. Optional 'observe' (default false) appends a fresh editor viewport capture to the result. Returns result with ok, path, type, button, double_click, window_id and position. Editor process only.",
               "Editor", std::vector<std::string>({"editor", "ui", "click"}), editor_ui_actions::handle_click_editor_element, true, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(TypeEditorElementTextTool, "type_editor_element_text",
               "Focus an editor UI element and write text into it by pushing the whole string as one text-input event instead of simulating keystrokes. Use it to fill editor fields such as search boxes or property inputs resolved through get_editor_ui_elements; the element is focused with grab_focus when it does not already have focus. Optional 'submit' (default false) appends an Enter press and release. Optional 'observe' (default false) appends a fresh editor viewport capture to the result. Returns result with ok, path, length, focused and submit. Editor process only.",
               "Editor", std::vector<std::string>({"editor", "ui", "type"}), editor_ui_actions::handle_type_editor_element_text, true, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(RunEditorShortcutTool, "run_editor_shortcut",
               "Inject an editor keyboard shortcut to run a command, e.g. 'ctrl+s' to save, 'ctrl+shift+z' to redo or 'f5' to play the project. The string is parsed as '+'-separated modifiers (ctrl/control, shift, alt, meta/super, case-insensitive) followed by the main key, which accepts the same names as the input tools plus F1-F12. The press and release are injected with the modifier flags set; a malformed shortcut or an unknown key returns an error. Returns result with ok, shortcut, key and the ctrl/shift/alt/meta flags. Editor process only.",
               "Editor", std::vector<std::string>({"editor", "shortcut", "input"}), editor_ui_actions::handle_run_editor_shortcut, true, ::godot_autopilot::SideEffect::ModifiesWindow)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(29);
  v.push_back(std::make_unique<GetEditorSelectionTool>());
  v.push_back(std::make_unique<SetEditorSelectionTool>());
  v.push_back(std::make_unique<GetEditorEditedSceneRootTool>());
  v.push_back(std::make_unique<SaveEditorSceneTool>());
  v.push_back(std::make_unique<SaveEditorScenesTool>());
  v.push_back(std::make_unique<ReloadEditorSceneTool>());
  v.push_back(std::make_unique<InspectEditorResourceTool>());
  v.push_back(std::make_unique<CreateEditorUndoRedoActionTool>());
  v.push_back(std::make_unique<CommitEditorUndoRedoTool>());
  v.push_back(std::make_unique<AddEditorUndoRedoDoTool>());
  v.push_back(std::make_unique<AddEditorUndoRedoUndoTool>());
  v.push_back(std::make_unique<GetEditorFileSystemTreeTool>());
  v.push_back(std::make_unique<ScanEditorFileSystemTool>());
  v.push_back(std::make_unique<SetEditorMainSceneTool>());
  v.push_back(std::make_unique<PlayEditorCurrentSceneTool>());
  v.push_back(std::make_unique<StopEditorPlayingTool>());
  v.push_back(std::make_unique<GetEditorFileSystemStatusTool>());
  v.push_back(std::make_unique<SetEditorPluginEnabledTool>());
  v.push_back(std::make_unique<CreateEditorSceneTool>());
  v.push_back(std::make_unique<OpenEditorSceneTool>());
  v.push_back(std::make_unique<SaveEditorSceneAsTool>());
  v.push_back(std::make_unique<BuildCsharpAssemblyTool>());
  v.push_back(std::make_unique<CloseEditorSceneTool>());
  v.push_back(std::make_unique<GetEditorViewportGeometryTool>());
  v.push_back(std::make_unique<GetEditorUiElementsTool>());
  v.push_back(std::make_unique<HitTestEditorPointTool>());
  v.push_back(std::make_unique<ClickEditorElementTool>());
  v.push_back(std::make_unique<TypeEditorElementTextTool>());
  v.push_back(std::make_unique<RunEditorShortcutTool>());
  return v;
}

} // namespace editor_tools
} // namespace godot_autopilot

#endif