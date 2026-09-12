# GDScript Workflows

Creating, attaching, editing and reloading GDScript through godot-autopilot, and the safety differences between the four execution channels plus the code_execute meta tool. Engine-level execution gotchas (placeholder instances, static initializers, editor-vs-game semantics, frame ordering) are summarized here and covered in depth in `references/execution-gotchas.md`.

## Script lifecycle

- `create_script` compiles before saving: compilation errors abort the call with parser output, so a success means the file on disk parses. `overwrite` defaults to false. The saved content is read back from disk and verified; a locked file surfaces as verified=false instead of silent success. Never issue parallel `create_script` calls for the same path - serial writes to one file are not synchronized. A passing compile only proves the file parses, not that the code runs correctly: after any script edit, read `get_debugger_log` to confirm the editor process logged no parser or runtime errors.
- `attach_script_to_node` and `detach_script_from_node` operate on the edited scene and are registered with the editor undo/redo. `script_path` accepts any resource that `ResourceLoader` resolves as a `Script`, not only .gd files. Replacing an existing script does not migrate or clean up the old script's exported properties: stored references such as `node_paths` may be lost and have to be re-wired by hand. A script without `@tool` cannot be instantiated in the editor, so its methods only run once the game runs - `call_script_node` reports the same restriction.
- `reload_script` reloads from disk after an external edit; `keep_state` (default false) preserves instance state across the reload. Static variables follow their own rule - see "Static initializers" below.
- `get_script_property_list` lists a script's declared variables with name, type and usage flags.
- `get_script_property` reads a value: with `script_path` it returns the declared default; with `node_path` the node's current value. Pass exactly one of the two.
- `set_script_property` writes a node property directly - no undo entry, no type or read-only validation. It is the lightweight channel for quick in-memory tweaks; use `property_set` when undo support and validation matter.

## Gotcha: non-@tool scripts never execute in the editor

The single most common cause of "my script did nothing" reports:

- A script without `@tool` is never instantiated in the editor process. A node holding such a script in the edited scene carries a placeholder instance (`PlaceHolderScriptInstance`) that exposes only the exported property defaults.
- Zero code runs: no `_init`, no `_ready`, no `_process`, no tool-time callbacks - and nothing errors or warns. The scene looks perfectly healthy while the script is inert.
- The gate is `can_instantiate = valid && !abstract && (tool || scripting_enabled)`: only scripts marked `@tool` (or failing compilation or abstract-ness checks) are instantiable in the editor.
- Symptoms: `get_script_property` with `script_path` keeps returning the declared defaults; editor-side mutations you expected from the script never appear; `call_script_node` is rejected for non-`@tool` scripts.
- Fix: add `@tool` as the first line, then `reload_script` - or run the game and use the game channel.

Details in `references/execution-gotchas.md`.

## Four execution channels

| Channel | Runs where | @tool needed | Timeout | Sweet spot |
|---|---|---|---|---|
| `execute_script` | editor process, synchronous | no | none - long code blocks the editor | editor automation with `SceneRoot` access |
| `call_script_node` | an existing node instance in the edited scene | yes | default tool timeout | calling one method with real instance state |
| `execute_game_script` | the running game process | no | runtime channel default | game-state queries and mutations |
| `code_execute` | editor process, wrapped temporary node | wrapped for you | `timeout_ms`, default 5000, cap 30000 | loops, bulk edits, computed results |

All four execute arbitrary GDScript and are treated as highest-risk side effects.

- `execute_script` auto-returns a single expression's value; multi-line code needs an explicit return. print() output lands in the output field, errors in the errors field. The environment exposes `SceneRoot` (the edited scene root); reach scene nodes via SceneRoot.get_node(...).
- `call_script_node` runs inside the existing node instance, so its state is real, not reconstructed. Its parameters are `node_path`, `function` (the method name — not `method`) and optional `args`. Non-`@tool` scripts error - run the game instead for those.
- `execute_game_script` runs inside the running game. Unlike the code_execute sandbox, temporary nodes it creates can persist in the game after the call returns. Game-channel prerequisites are covered by the godot-autopilot-runtime skill.

## The code_execute meta tool

code_execute wraps your source into a `@tool` extends Node script, runs it on a temporary editor node, then cleans the node up.

- Single-function mode (default): your code is indented into an implicit entry function, so top-level func definitions are not supported. Write straight-line code ending in return:

```gdscript
var total = 0
for child in SceneRoot.get_children():
    total += 1
return total
```

- Multi-function mode: when the source contains func definitions they are preserved; pick the entry point with `function_name`:

```gdscript
func build_report():
    var lines = []
    for child in SceneRoot.get_children():
        lines.append(child.name)
    return lines
```

Pass `function_name` set to build_report to select it.

- Indentation (tabs or spaces) is auto-detected per source; mixing both aborts with an error.
- `timeout_ms` defaults to 5000 and is clamped to 30000. The check runs once before execution: synchronous GDScript cannot be interrupted, so an infinite loop still blocks the editor until it returns.
- A source calling `close_scene(` is rejected outright - it would destroy the node executing the code and crash the editor. Use `close_editor_scene` to close scenes.
- `SceneRoot` is injected as the edited scene root; the wrapper node itself lives under /root, so scene nodes are reachable only through SceneRoot.get_node(...).
- Node cleanup: with `auto_owner`=true, nodes created during execution get their owner assigned recursively and survive (reported in auto_owner_set); with false, newly created ownerless children are deleted after the run.
- Results: the return value is serialized into result, alongside execution_time_ms, output (capped at 8192 bytes) and wrapped_source; compile errors arrive with line-mapped messages and the wrapped source. When the result is a Resource it is auto-registered and returned through registered_resource.

## Bulk data through code_execute

Anything touching many objects - filling tile cells, rewriting hundreds of properties, computing over large arrays - belongs in one code_execute loop instead of hundreds of individual tool calls. It avoids oversized JSON payloads (responses cap at 4 MiB) and runs far faster.

## Static initializers

- `_static_init` runs after every successful script reload, not once per session: side effects placed there (building caches, registering helpers) re-execute on each `reload_script`.
- `self` is unavailable inside `_static_init` - it executes in a static context with no instance.
- Static variables survive a hot reload only when `reload_script` runs with `keep_state=true`; a plain reload re-initializes them (and re-runs `_static_init` on top).

## Editor vs game process semantics

`is_editor_hint()` is `true` for the whole editor process - including `@tool` scripts and editor plugins - and `false` for the entire F5-launched game subprocess. Two frequent knock-on effects: `Input.set_custom_mouse_cursor()` is a no-op when called in the editor process, and editor-only (API_EDITOR) classes error when instantiated in the running game. Branch on `is_editor_hint()` early in `@tool` code that must behave differently per process. Full cross-process details: `references/execution-gotchas.md`.

## Tree entry and exit order

For any subtree added to or removed from the scene tree, callbacks fire in a fixed three-phase order:

- `_enter_tree`: parents before children.
- `_ready`: children before parents - a parent's `_ready` sees all descendants fully initialized.
- `_exit_tree`: children before parents.

The `tree_exited` signal fires once after the whole branch has been cut from the tree, not per node during removal. In a `@tool` script the same order applies to editor-side tree changes.

## C# limitations

C#-specific workflows — `build_csharp_assembly`, the lack of in-editor .NET assembly hot-reload, and how C# properties appear to GDScript — live in the `godot-autopilot-csharp` skill; the GDScript channels are unaffected by .NET assembly state.

## Hot reload in the running game

`reload_game_scripts` reloads GDScript files inside the running game without restarting it. Use it after editing scripts mid-session, then re-run your verification calls. Remember the static-variable rule above applies to editor reloads; in-game reload behavior is covered by the godot-autopilot-runtime skill.

## Choosing a channel

- Editor-side inspection or one-shot automation: `execute_script`.
- One method on one node with real instance state: `call_script_node` (`@tool` required).
- Anything inside the running game: `execute_game_script`.
- Loops, bulk edits, computed results, anything wanting a timeout guard: `code_execute`.

## See also

- `godot-autopilot-runtime` - the game channel, input injection, pause semantics and status
- `godot-autopilot-scene-system` - node CRUD, property JSON shapes and undo semantics
- `godot-autopilot` - discovery protocol and the error watermark
