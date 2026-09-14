# Category Index

All 366 domain tools of the godot-autopilot MCP server, grouped by their 30 source modules. Call domain tools through `call_tool`; confirm schemas with `get_tool_detail`.

## Analysis - analyze_tools (3)

- `validate_scene_file` - dry-run validate a scene file on disk without touching the edited scene
- `find_unused_resources` - find project files never referenced by any other file's dependencies
- `trace_signal_flow` - trace signal wiring around a node of the edited scene

## Animation - animation_tools (10)

- `create_scene_animation_player` - create an AnimationPlayer node in the edited scene
- `create_animation` - create an Animation resource in a player's default library
- `remove_animation` - remove a named animation from an AnimationPlayer
- `get_animation_list` - list a player's animations with length and loop mode
- `create_animation_track` - add a value or method track to an animation
- `insert_animation_keyframe` - insert one key into a track at a given time
- `remove_animation_track` - remove a whole track from an animation
- `create_scene_animation_tree` - create an AnimationTree with an empty state machine root
- `add_animation_machine_state` - append a state to the AnimationTree state machine
- `connect_animation_states` - connect two states, optionally with a transition condition

## Audio - audio_tools (20)

- `get_audio_bus_layout` - read the complete audio bus layout including effect chains
- `set_audio_bus_layout` - replace the whole audio bus layout
- `get_audio_bus_count` - count audio buses including Master
- `get_audio_bus_name` - read a bus name by zero-based index
- `set_audio_bus_volume_db` - set a bus volume in decibels
- `set_audio_bus_mute` - mute or unmute a bus
- `set_audio_bus_bypass_effects` - bypass a bus's whole effect chain without removing it
- `add_audio_bus_effect` - add an audio effect to a bus by class name
- `remove_audio_bus_effect` - remove a bus effect by index
- `set_audio_bus_solo` - solo a bus so only it is audible
- `play_audio_player` - play a stream on an audio player node
- `stop_audio_player` - stop an audio player node
- `set_audio_player_volume_db` - set a player's volume in decibels
- `set_audio_player_pitch_scale` - set a player's pitch scale
- `get_audio_player_playback_position` - read a player's playback position in seconds
- `seek_audio_player` - seek a player to a position in seconds
- `get_audio_device_outputs` - list available audio output devices
- `set_audio_device_output` - switch the editor's audio output device
- `get_audio_device_inputs` - list available audio input devices
- `set_audio_device_input` - switch the editor's audio input device

## Capture - capture_tools (1)

- `capture_editor_viewport` - screenshot the editor viewport (or the running game with target=game) as base64 PNG; save=true on the editor target also writes the PNG under user://godot_autopilot/captures/ and returns its path (only the 20 most recent captures are kept); through `call_tool` the PNG is delivered as image content (`data` becomes `<attached-as-image-content>` with image_attached: true)

## Config - config_tools (13)

- `get_project_settings` - read one project.godot setting
- `set_project_settings` - set a project setting in memory only (call save afterwards)
- `has_project_settings` - check whether a project setting exists
- `save_project_settings` - persist in-memory project settings to project.godot
- `get_engine_version` - read the running engine version
- `get_engine_fps` - measure the current frames per second
- `get_engine_frames_drawn` - read the total frames-drawn counter
- `set_engine_time_scale` - scale the game loop speed (slow motion or fast forward)
- `get_engine_time_scale` - read the current time scale
- `set_engine_max_fps` - cap the frame rate (0 disables the cap)
- `get_editor_settings` - read one editor preference
- `set_editor_settings` - set an editor preference (persists automatically)
- `has_editor_settings` - check whether an editor preference exists

## Debug - debug_tools (15)

- `print_debug_log` - log a message through the plugin log system
- `get_debug_stack` - capture script backtraces in the editor process
- `get_debug_monitor` - read one built-in performance monitor by id
- `get_debug_monitor_catalog` - list built-in performance monitors with ids
- `get_debug_monitors` - read all built-in monitors in one snapshot
- `get_debug_custom_monitor` - read one custom performance monitor by id
- `get_debug_custom_monitor_names` - list registered custom monitor ids
- `remove_debug_custom_monitor` - remove a custom monitor by id
- `get_debug_object_count` - read the live Object count
- `get_debug_node_count` - read the live Node count
- `get_debug_memory_usage` - read current static memory usage in bytes
- `set_debug_physics_fps` - set the physics ticks-per-second rate
- `set_debug_collision_visual` - toggle collision shape visualization
- `set_debug_navigation_visual` - toggle navigation geometry visualization
- `set_debug_performance_visual` - toggle the performance overlay

## Debugger - debugger_tools (6)

- `get_debugger_log` - read the editor engine log buffer (works without a running game)
- `get_plugin_log` - read the plugin's own in-process log buffer (authorization denials, timeout diagnostics, dropped late game responses; supports `limit`, `level`, `category`, `filter` and `since_index`)
- `get_debugger_errors` - read script errors captured from the running game
- `get_debugger_output` - read game stdout/stderr over the runtime channel
- `get_debugger_scene_tree` - read the running game's scene tree as text
- `get_debugger_session_info` - read debug session state (active or stopped at a breakpoint)

## Display - display_tools (24)

- `get_display_clipboard` - read the system clipboard text
- `set_display_clipboard` - write text to the system clipboard
- `show_display_dialog` - show a native modal dialog (user-visible side effect)
- `get_display_mouse_position` - read the mouse cursor position in screen coordinates
- `set_display_mouse_mode` - set cursor behavior (visible, captured, hidden)
- `warp_display_mouse` - move the mouse cursor to a position in the focused window's client area (not screen coordinates)
- `capture_display_screen` - screenshot a physical screen as PNG
- `get_display_screen_count` - count connected screens
- `get_display_screen_dpi` - read a screen's DPI
- `get_display_screen_position` - read a screen's desktop position
- `get_display_screen_refresh_rate` - read a screen's refresh rate in hertz
- `get_display_screen_size` - read a screen's pixel size
- `get_display_tts_voices` - list available text-to-speech voices
- `speak_display_tts` - speak text aloud via text-to-speech
- `stop_display_tts` - stop ongoing text-to-speech playback
- `create_display_window` - create an editor sub-window and return its id
- `delete_display_window` - close a previously created sub-window
- `move_display_window_to_foreground` - raise a window and give it focus
- `request_display_window_attention` - flash a window's taskbar entry
- `set_display_window_flag` - set a window flag such as always-on-top
- `set_display_window_mode` - set a window mode such as fullscreen or maximized
- `set_display_window_position` - move a window to screen coordinates
- `set_display_window_size` - resize a window in pixels
- `set_display_window_title` - set a window's title bar text

## Docs - doc_tools (4)

- `get_docs_class` - reflect a Godot class signature from ClassDB (no docstrings)
- `find_docs_class` - substring search over registered class names
- `get_docs_method` - look up one method's reflected signature on a class
- `get_docs_property` - look up one property's reflected info on a class

## Editor - editor_tools (23)

- `get_editor_selection` - list the currently selected nodes
- `set_editor_selection` - replace the editor selection with given paths
- `get_editor_edited_scene_root` - fetch the edited scene's root node info
- `save_editor_scene` - save the currently edited scene to disk
- `save_editor_scenes` - save every open scene tab at once
- `save_editor_scene_as` - save the current scene to a specific path
- `reload_editor_scene` - reload a scene from disk, discarding unsaved edits
- `close_editor_scene` - close the currently edited scene
- `create_editor_scene` - create a new empty scene with a root node of a given type
- `open_editor_scene` - open an existing scene file in the editor
- `inspect_editor_resource` - open a resource file in the Inspector panel
- `create_editor_undo_redo_action` - start a custom undo/redo action
- `add_editor_undo_redo_do` - register a do-step method call in the open action
- `add_editor_undo_redo_undo` - register an undo-step method call in the open action
- `commit_editor_undo_redo` - commit the open action as a single undo step
- `get_editor_file_system_tree` - fetch the project file system directory tree
- `get_editor_file_system_status` - report file system scan state and progress
- `scan_editor_file_system` - trigger a full file system rescan
- `set_editor_main_scene` - set the project's main scene
- `set_editor_plugin_enabled` - enable or disable an editor plugin
- `play_editor_current_scene` - launch the edited scene as a game
- `stop_editor_playing` - stop the running game process
- `build_csharp_assembly` - trigger an async dotnet build of the C# project

## Game - game_tools (9)

- `get_game_status` - query the running game's engine stats over the runtime channel
- `execute_game_script` - run code inside the running game process
- `reload_game_scripts` - reload GDScripts in the running game without restarting it
- `queue_game_input` - inject input into the running game (the editor-side input tools do not reach it)
- `wait_game_input` - wait for a transient input state on an action in the game
- `get_game_input_status` - query pressed and just-pressed state in the game
- `sequence_game_inputs` - schedule up to 256 input events on exact physics frame offsets
- `capture_game_viewport` - screenshot the running game as base64 PNG
- `get_game_ui_elements` - enumerate Control nodes of the running game for click automation

## Group - group_tools (3)

- `add_group_node` - add a node to a group persistently (saved with the scene, undo-tracked)
- `remove_group_node` - remove a node from a group (undo-tracked)
- `has_group_node` - check whether a node belongs to a group

## Input - input_tools (11)

- `press_input_action` - press an input action in the editor process
- `release_input_action` - release a previously pressed input action
- `is_input_action_pressed` - check whether an action is currently held in the editor
- `is_input_action_just_pressed` - check the single-frame just-pressed state in the editor
- `press_input_key` - inject a key press into the editor (restricted key set)
- `release_input_key` - inject a key release into the editor
- `move_input_mouse` - inject mouse motion into the editor process
- `press_input_mouse_button` - inject a left, right or middle mouse press into the editor
- `release_input_mouse_button` - inject a mouse button release into the editor
- `start_input_gamepad_vibration` - start gamepad vibration on the editor process
- `stop_input_gamepad_vibration` - stop gamepad vibration

## Input - input_map_tools (8)

- `get_input_map_actions` - list all input action names of the editor project
- `has_input_map_action` - check whether an input action exists
- `add_input_map_action` - add an empty input action (deadzone defaults to 0.5)
- `add_input_map_action_event` - bind an input event object to an action
- `erase_input_map_action_event` - remove one bound event from an action by index
- `set_input_map_action_deadzone` - set an action's analog deadzone
- `erase_input_map_action` - remove an action and its persisted setting
- `save_input_map` - persist the editor InputMap to project settings

## Navigation - nav_tools (15)

- `create_nav_2d_map` - create a 2D navigation map and return its RID
- `create_nav_2d_region` - create a 2D navigation region on a map
- `get_nav_2d_map_path` - query a navigation path across a 2D map
- `create_nav_2d_agent` - create a 2D avoidance agent on a map
- `set_nav_2d_agent_velocity` - drive a 2D agent's velocity each frame
- `create_nav_3d_map` - create a 3D navigation map and return its RID
- `set_nav_3d_map_cell_size` - set a 3D map's cell size
- `create_nav_3d_region` - create a 3D navigation region on a map
- `set_nav_3d_region_navigation_mesh` - assign a NavigationMesh resource to a 3D region
- `get_nav_3d_map_path` - query a navigation path across a 3D map
- `get_nav_3d_map_closest_point_to_segment` - find the closest map point to a segment
- `create_nav_3d_agent` - create a 3D avoidance agent on a map
- `set_nav_3d_agent_velocity` - drive a 3D agent's velocity each frame
- `get_nav_3d_agent_state` - read a 3D agent's position and velocity
- `create_nav_3d_obstacle` - create a 3D obstacle avoidance agents steer around

## OS - os_tools (18)

- `show_os_alert` - show a blocking native alert dialog (blocks the editor UI)
- `create_os_process` - start a detached background process and return its PID
- `execute_os_process` - run a command synchronously and capture stdout/stderr
- `kill_os_process` - force-kill a process by PID
- `open_os_path` - open a path or URL with the default application
- `get_os_datetime` - read current date and time fields
- `get_os_unix_time` - read Unix time as floating-point seconds
- `get_os_locale` - read the system locale string
- `get_os_system_fonts` - list installed system font names
- `get_os_system_info` - read host machine and OS information
- `get_os_unique_id` - read the machine unique identifier
- `get_os_user_data_dir` - read the project's user data directory path
- `get_os_environment` - read an editor process environment variable
- `set_os_environment` - set an editor process environment variable
- `write_file` - write or append text to a file under res:// or user://
- `read_file` - read a text file from disk
- `find_in_files` - recursively search text files for a query string
- `move_os_file_to_trash` - move a file or folder to the system trash

## Physics - physics_tools (48)

- `get_physics_2d_space_direct_state` - read a 2D physics space's direct state before queries
- `intersect_physics_2d_ray` - cast a 2D ray and return the first hit (space auto-detected from the edited scene)
- `intersect_physics_2d_shape` - intersect a 2D space with a shape RID
- `intersect_physics_2d_point` - query colliders touching a 2D point
- `create_physics_2d_body` - create a server-side 2D body and return its RID
- `set_physics_2d_body_mode` - set a 2D body's simulation mode
- `set_physics_2d_body_state` - set a 2D body state field such as transform or velocity
- `get_physics_2d_body_state` - read a 2D body state field
- `apply_physics_2d_body_force` - apply a continuous force to a 2D body
- `apply_physics_2d_body_impulse` - apply an instantaneous impulse to a 2D body
- `create_physics_2d_joint` - create a 2D joint (pin, groove or damped spring)
- `create_physics_2d_area` - create a 2D physics area and return its RID
- `set_physics_2d_area_monitorable` - set whether a 2D area can be monitored
- `create_physics_2d_circle_shape` - create a 2D circle shape and return its RID
- `set_physics_2d_shape_data` - set a 2D circle shape's geometry
- `get_physics_3d_space_direct_state` - read a 3D physics space's direct state before queries
- `intersect_physics_3d_ray` - cast a 3D ray and return the first hit (space RID required)
- `intersect_physics_3d_shape` - intersect a 3D space with a shape RID
- `intersect_physics_3d_point` - query colliders touching a 3D point
- `create_physics_3d_body` - create a server-side 3D body and return its RID
- `set_physics_3d_body_mode` - set a 3D body's simulation mode
- `set_physics_3d_body_state` - set a 3D body state field
- `get_physics_3d_body_state` - read a 3D body state field
- `set_physics_3d_body_transform` - teleport a 3D body to a transform
- `apply_physics_3d_body_force` - apply a continuous force to a 3D body
- `apply_physics_3d_body_impulse` - apply an instantaneous impulse to a 3D body
- `apply_physics_3d_body_torque` - apply rotational torque to a 3D body
- `set_physics_3d_body_axis_lock` - lock a movement axis on a 3D body
- `set_physics_3d_body_param` - set a 3D body parameter such as mass by enum index
- `add_physics_3d_body_shape` - attach a shape RID to a 3D body
- `add_physics_3d_body_collision_exception` - stop a 3D body colliding with another
- `remove_physics_3d_body_collision_exception` - re-enable collision between two bodies
- `create_physics_3d_joint` - create a 3D joint (pin, hinge, slider, cone twist or 6-DOF)
- `set_physics_3d_joint_param` - set solver parameters on a 3D joint
- `create_physics_3d_area` - create a 3D physics area and return its RID
- `set_physics_3d_area_monitorable` - set whether a 3D area can be monitored
- `set_physics_3d_area_param` - set a 3D area parameter such as gravity by enum index
- `set_physics_3d_area_space` - attach a 3D area to a physics space
- `set_physics_3d_area_transform` - place a 3D area with a global transform
- `set_physics_3d_space_param` - set a 3D space parameter by enum index
- `set_physics_3d_space_solver_iterations` - set a 3D space's solver iteration count
- `set_physics_3d_space_solver_params` - set solver iterations and penetration limits together
- `create_physics_3d_soft_body` - create a 3D soft body and return its RID
- `set_physics_3d_soft_body_mesh` - assign a mesh to a 3D soft body
- `create_physics_3d_sphere_shape` - create a sphere shape and return its RID
- `set_physics_3d_shape_data` - set a sphere shape's geometry
- `get_physics_node_rid` - get the RID of a scene physics node for server-side tools
- `get_debug_object_info` - resolve an ObjectID to its class and identity

## Properties - property_tools (5)

- `property_get` - read one property value from a scene node
- `property_get_list` - list every property of a node with metadata
- `property_set` - set a node property (editor undo, errors carry candidate suggestions)
- `signal_connect` - connect a signal to a target method (persisted with the scene by default)
- `signal_disconnect` - remove a signal connection (idempotent)

## Render - render_tools (49)

- `create_render_canvas_item` - create a server-side 2D canvas item and return its RID
- `add_render_canvas_item_rect` - draw a filled rectangle on a canvas item
- `add_render_canvas_item_circle` - draw a filled circle on a canvas item
- `add_render_canvas_item_texture_rect` - draw a texture into a rectangle on a canvas item
- `add_render_canvas_item_line` - draw a line on a canvas item
- `set_render_canvas_item_transform` - set a canvas item's 2D transform
- `set_render_canvas_item_visible` - set a canvas item's visibility
- `get_render_canvas_item_rid` - get the canvas item RID of a CanvasItem node
- `create_render_scenario` - create a server-side 3D scenario and return its RID
- `set_render_scenario_environment` - attach an environment to a 3D scenario
- `create_render_camera` - create a server-side 3D camera and return its RID
- `set_render_camera_transform` - set a 3D camera's position
- `set_render_camera_perspective` - set a perspective projection on a camera
- `set_render_camera_orthogonal` - set an orthogonal projection on a camera
- `create_render_light` - create a directional, omni or spot light and return its RID
- `set_render_light_param` - set a numeric parameter on a light
- `set_render_light_color` - set a light's color
- `create_render_mesh` - create a server-side mesh and return its RID
- `add_render_mesh_surface` - add a vertex-array surface to a mesh
- `set_render_mesh_surface_material` - assign a material to one mesh surface
- `create_render_material` - create a server-side material and return its RID
- `set_render_material_param` - set a named parameter on a material
- `create_render_viewport` - create a server-side viewport and return its RID
- `set_render_viewport_size` - set a viewport's pixel size
- `set_render_viewport_clear_mode` - set how a viewport clears each frame
- `create_render_particles` - create a 2D or 3D particle system and return its RID
- `set_render_particles_emitting` - start or stop particle emission
- `restart_render_particles` - restart particle emission from the beginning
- `set_render_particles_lifetime` - set the particle lifetime in seconds
- `set_render_environment_bg_color` - set an environment's background color
- `set_render_environment_ambient_light` - set an environment's ambient light
- `set_render_environment_glow` - configure glow (bloom) post-processing
- `set_render_environment_ssr` - configure screen-space reflections
- `set_render_environment_tonemap` - configure tone mapping and exposure
- `set_render_environment_sdfgi` - configure SDFGI global illumination
- `set_render_environment_volumetric_fog` - configure volumetric fog
- `create_render_fog_volume` - create a volumetric fog volume and return its RID
- `set_render_fog_volume_shape` - set a fog volume's shape after creation
- `create_render_shader` - create a server-side shader, optionally with source code
- `set_render_shader_code` - replace a shader's source code
- `set_render_shader_parameter_global` - set a global shader parameter
- `create_render_texture_from_image` - create a texture from an image file and return its RID
- `create_render_sky` - create a server-side sky and return its RID
- `set_render_sky_material` - set a sky's material
- `create_render_reflection_probe` - create a reflection probe and return its RID
- `create_render_decal` - create a decal and return its RID
- `set_render_instance_visible` - set a rendering instance's visibility
- `set_render_instance_layer_mask` - set a rendering instance's camera layer mask
- `find_render_node_from_rid` - validate an RID and find scene nodes using it

## Resources - resource_tools (26)

- `load_resource` - load a resource file from disk into memory
- `reload_resource` - force-reload a resource file from disk, replacing the editor's cached instance
- `load_resource_threaded` - start an async background load (step 1 of the threaded chain)
- `get_resource_load_threaded_status` - poll a threaded load's status (step 2)
- `get_resource_load_threaded` - fetch a threaded load's result (step 3)
- `save_resource` - save a resource to disk, including memory:// instances
- `create_resource` - instantiate a Resource subclass in memory, optionally registered as memory://name
- `duplicate_resource` - load and duplicate a file-backed resource
- `get_resource_type` - report the engine class name of the resource at a path
- `get_resource_types` - list every instantiable Resource subclass
- `get_resource_extensions` - list file extensions recognized for a resource type
- `has_resource` - check whether a path is loadable as a resource
- `get_resource_dir_files` - list entries of a res:// or user:// directory
- `get_resource_uid` - read the numeric UID of a resource file
- `set_resource_uid` - assign or auto-generate a UID for a resource file
- `remove_resource_file` - delete a resource file (dry-run scan first, force to actually delete)
- `rename_resource_file` - rename or move a file with automatic reference rewriting
- `move_resource_file` - move a file to another res:// directory through the same transaction pipeline
- `copy_resource_file` - copy a file to dest_path inside res:// (byte-for-byte; .import/.uid sidecars are not copied)
- `create_directory` - create directories inside the project (res:// only)
- `get_resource_dependencies` - list the external files a resource references
- `has_resource_dependency` - check whether a resource depends on a specific file
- `get_resource_references` - reverse lookup: which files reference a resource
- `reimport_resource_files` - queue an async reimport through the editor file system
- `set_resource_property` - set a property on a located resource
- `get_resource_property` - read a property from a located resource

## Scene - scene_tools (6)

- `create_scene_node` - create a node in the edited scene with optional type, name, parent and inline properties
- `delete_scene_node` - delete a node via editor undo/redo (refuses the scene root)
- `rename_scene_node` - rename a node, refusing duplicate sibling names
- `reparent_node` - move a node under a new parent as one undo step
- `get_scene_tree` - walk the edited scene tree with optional depth and properties
- `instantiate_scene` - instantiate a .tscn PackedScene into the edited scene

## Scene - scene_tree_tools (8)

- `call_scene_tree_group` - call a method on every node in a group
- `notify_scene_tree_group` - send a Godot notification to every node in a group
- `get_scene_tree_nodes_in_group` - list node paths of all nodes in a group
- `create_scene_tree_timer` - create a SceneTreeTimer for delayed logic
- `is_scene_tree_paused` - check whether the running scene tree is paused
- `set_scene_tree_pause` - pause or unpause the running scene tree
- `set_scene_tree_debug_collisions_hint` - toggle collision shape debug visualization
- `reload_scene_tree_current_scene` - reload the currently running scene from disk

## Scripts - script_tools (10)

- `execute_script` - execute GDScript synchronously in the editor process
- `create_script` - create a GDScript file on disk after compile-checking it
- `load_script` - load a GDScript resource from a res:// path
- `reload_script` - reload a script from disk, optionally keeping instance state
- `attach_script_to_node` - attach a GDScript file to a node (undo-tracked)
- `detach_script_from_node` - remove a node's script (undo-tracked)
- `call_script_node` - call a method on a node whose script is marked as a tool script
- `get_script_property` - read a script's declared default or a node's current property value
- `set_script_property` - set a node property without undo entry or validation
- `get_script_property_list` - list the declared variables of a GDScript file

## SpriteFrames - spriteframes_tools (3)

- `create_spriteframes` - create an in-memory SpriteFrames resource (built-in default animation removed)
- `add_spriteframes_animation` - add a named animation to a SpriteFrames resource
- `add_spriteframes_frame` - add frames to an animation, optionally splitting a sprite sheet

## System - system_tools (1)

- `get_game_log_entries` - read the tail of the game's on-disk log file (works without a debug session); an optional case-sensitive substring filter keeps matching lines from the last 2000 and reports the match count

## Testing - test_tools (2)

- `run_gdscript_tests` - run inline GDScript tests with the built-in assertion framework
- `run_gdscript_test_files` - run GDScript test files from a project directory

## Text - text_tools (10)

- `create_text_font` - create a TextServer font and return its id
- `create_shaped_text` - create a shaped text object and return its id
- `set_text_font_data` - load a font file into a font id
- `set_text_font_antialiasing` - set a font's antialiasing mode
- `set_text_font_hinting` - set a font's hinting mode
- `get_text_font_system_path` - resolve a system-installed font to an absolute file path
- `has_text_feature` - check whether the active TextServer supports a shaping feature
- `is_text_locale_right_to_left` - check whether a locale uses right-to-left direction
- `add_shaped_text_string` - add a text string to a shaped text object
- `get_shaped_text_size` - measure a shaped text object's rendered size in pixels

## Theme - theme_tools (8)

- `create_theme_resource` - create an empty .tres theme file on disk
- `set_theme_color` - set a color item on a saved theme
- `set_theme_constant` - set an integer constant item on a saved theme
- `set_theme_font_size` - set a font size item on a saved theme
- `set_theme_stylebox_flat` - build and register a StyleBoxFlat item on a saved theme
- `get_theme_info` - inspect a saved theme's types and items
- `apply_theme_to_control` - assign a .tres theme to a Control node in the edited scene
- `set_control_anchor_preset` - apply one of the 16 Control layout presets

## TileMap - tilemap_tools (4)

- `create_tilemap` - create a TileMap node with a default TileSet of a given tile size
- `create_tilemap_tileset` - create a TileSet resource in memory and register it as memory://name
- `set_tilemap_cell` - set a single cell on a TileMap or TileMapLayer node
- `set_tilemap_cells` - set multiple cells in one call; no fixed entry cap (larger batches: use code_execute); source_id -1 clears a cell

## TileMap - tileset_tools (3)

- `add_tilemap_atlas_source` - add an atlas source with a texture to an in-memory TileSet
- `add_tilemap_physics_layer` - add a physics layer to an in-memory TileSet
- `set_tilemap_tile_collision` - set collision polygons for one tile of a TileSet

## See also

- godot-autopilot - usage overview, discovery protocol and the error watermark
