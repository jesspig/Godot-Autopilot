# GDScript Workflows

Creating, attaching, editing, and running GDScript, plus the four execution
channels and their safety differences.

## Script lifecycle

- `create_script` compiles before saving; errors abort with parser output. It
  reads the file back and verifies it; a locked file reports `verified: false`.
  Never write the same path in parallel. A passing compile proves only parsing;
  after an edit, read `get_debugger_stream` action=log for parser or runtime
  errors.
- `attach_script_to_node` and `detach_script_from_node` are undoable.
  `script_path` accepts any resource that resolves as a `Script`. Replacing a
  script does not migrate its exported properties; stored references may be lost.
- `reload_script` reloads from disk; `keep_state` defaults to false.
- `get_script_property_list` lists declared variables. `get_script_property`
  reads a declared default (`script_path`) or a node value (`node_path`); pass
  one. `set_script_property` writes without undo or type validation;
  use `property_set` when those matter.
- `patch_script` replaces or inserts at one anchored spot and compiles before
  writing; preview with `dry_run`.

## Non-@tool scripts are inert in the editor

A script without `@tool` is never instantiated in the editor process. Its node
carries a placeholder exposing only exported defaults. No `_init`, `_ready`,
`_process`, or tool callback runs, and nothing warns. `get_script_property`
keeps returning declared defaults, and `call_script_node` rejects the script.
Add `@tool` as the first line, then `reload_script`, or run the game and use the
game channel.

## Four execution channels

| Channel | Runs in | @tool | Timeout | Sweet spot |
|---|---|---|---|---|
| `execute_script` | Editor, synchronous | No | None; blocks the editor | Editor automation with `SceneRoot` |
| `call_script_node` | Existing node in the edited scene | Yes | Default tool timeout | One method with real state |
| `game_script` action=execute | Running game | No | `timeout_ms` default 5000, cap 25000 | Game-state queries and mutations |
| `code_execute` | Editor, wrapped temporary node | Wrapped | `timeout_ms` default 5000, schema cap | Loops, bulk edits, computed results |

All four execute arbitrary GDScript and are highest-risk side effects.

- `execute_script` auto-returns a single expression; multi-line code needs an
  explicit return. Bare statements are wrapped into `extends Node` +
  `func _run()`; a complete script must define `func _run()`. Compile errors are
  line-mapped. `print()` output lands in `output`, errors in `errors`, and
  `SceneRoot` is the edited scene root.
- `call_script_node` runs inside one existing node, so state is real. Parameters
  are `node_path`, `function`, and optional `args`; non-`@tool` scripts error.
- `game_script` action=execute runs inside the running game with
  `script_action: "script"`. `source_code` accepts a complete GDScript defining
  `func _run()` or bare statements wrapped the same way. A value needs an
  explicit return; wrapped calls report `wrapped: true`. Temporary nodes it
  creates can persist after the call.

## code_execute

`code_execute` wraps the source into a `@tool` script, runs it on a temporary
editor node, then cleans up.

- Single-function mode: no line starts with `func ` (trailing space), so code is
  indented into an implicit entry function. Write straight-line code ending in
  return.
- Multi-function mode: a `func ` line preserves the source; select the entry
  with `function_name` (default `_run`). `func(` or `func` plus tab does not
  select this mode.
- Indentation is auto-detected; mixing tabs and spaces aborts.
- `timeout_ms` defaults to 5000 and is clamped to the schema maximum; the
  deadline is checked while the script runs.
- A source calling `close_scene(` is rejected. Use `editor_scene_control`
  action=close.
- `SceneRoot` is the edited scene root; the wrapper lives under `/root`, so
  reach scene nodes through `SceneRoot.get_node(...)`.
- With `auto_owner` true, nodes created during execution get owners recursively
  and survive; with false, new ownerless children are deleted after the run.
- The return value is serialized into `result`, with `execution_time_ms`,
  `output` (capped at 8192 bytes), and `wrapped_source`. A Resource result is
  returned as `registered_resource`.

Use one `code_execute` loop for bulk work instead of many calls; it
avoids oversized JSON payloads and runs faster.

## Static initializers

`_static_init` runs after every successful reload, not once per session, so
caches and registrations re-execute on each `reload_script`. `self` is
unavailable inside it. Static variables survive a hot reload only with
`keep_state=true`; a plain reload re-initializes them and re-runs
`_static_init`.

## Editor vs game process

`is_editor_hint()` is true for the whole editor process, including `@tool`
scripts and editor plugins, and false for the launched game process.
`Input.set_custom_mouse_cursor()` is a no-op in the editor, and editor-only
classes error in the game. Branch early in `@tool` code that must differ.

## Tree entry and exit order

`_enter_tree` runs parents before children; `_ready` runs children before
parents; `_exit_tree` runs children before parents. `tree_exited` fires once
after the whole branch is cut. Stage required children before the parent enters,
or resolve lazily with `get_node_or_null` guards.

## Choosing a channel

- Editor inspection or one-shot automation: `execute_script`.
- One method on one node with real state: `call_script_node` (`@tool` required).
- Anything inside the running game: `game_script` action=execute.
- Loops, bulk edits, computed results, or a timeout guard: `code_execute`.

## Failure signatures

| Symptom | Cause | First move |
|---|---|---|
| Script does nothing in the editor | Missing `@tool` | Add `@tool`, then `reload_script` |
| `call_script_node` rejected | Script is not `@tool` | Run the game channel |
| `create_script` succeeded but code fails | Compile only proves parsing | Read `get_debugger_stream` action=log |
| Editor freezes during `execute_script` | No timeout on that channel | Move long work to `code_execute` |
| Static cache rebuilt on every reload | `_static_init` reruns | Move side effects out |
