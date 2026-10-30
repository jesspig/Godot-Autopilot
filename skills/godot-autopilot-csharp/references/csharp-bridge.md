# C# Reflection Bridge

The reflection tools and the two bridge endpoints behind them.
`csharp_list_members` discovers members, `csharp_get_property` reads a C#
property or field, `csharp_set_property` writes one, `csharp_call_method` calls
a method, `csharp_subscribe_event` wires a receiver method to a C# event and
`csharp_unsubscribe_event` releases that wiring. Domain tools run through
`call_tool`; confirm schemas with `get_tool_detail`. Reach for this reference
when GDScript-side reads (`property_get`, `get_script_property`,
`execute_game_script`) come back null or missing for a member you know exists
in C#.

## Endpoints

- Editor (`runtime` editor, the default): the locator is resolved in the editor
  process and the call goes to the `GdaCSharpBridge` engine singleton. If the
  singleton is absent the plugin loads `res://addons/godot-autopilot/GdaCSharpBridge.cs`
  and instantiates it on demand, so editor-side reflection works without any
  manual registration.
- Game (`runtime` game): only scene-path locators are accepted, and the call is
  forwarded over the runtime channel to the game bridge. Needs a running .NET
  game whose scene contains `GdaCSharpRuntime`; `object_id`, `object_id_str` and
  `memory` locators are rejected because editor-process handles cannot cross
  into the game.

## Locators

`path` resolves in this order:

- a `res://` path loads the resource; a resource that is a `Script` is rejected
  with "resolved to a Script resource, not a live object" - pass the node path
  (for example `World/Player`) or the `object_id_str` of a live instance.
- a `memory://` value must use the `memory` parameter, not `path`.
- anything else is a scene node path resolved against the edited scene root
  (editor) or the running tree (game).

## Bridge loading and the runtime node

- Editor: the bridge singleton is loaded lazily from
  `res://addons/godot-autopilot/GdaCSharpBridge.cs`; the file must exist in the
  project (deploy copies it) and be part of the .NET project so it compiles.
- Game: add a `GdaCSharpRuntime` node anywhere in the game scene. Its ready
  step creates a child node named `__gda_runtime` and stores a fresh
  `GdaCSharpBridge` instance in that child's metadata. Failures are precise:
  no `__gda_runtime` node means the runtime node is missing; a holder without
  bridge metadata means its ready step did not run.
- The `__gda_runtime` holder is GDA instrumentation: `get_game_status.node_count`
  excludes it and reports the excluded count as `instrumentation_node_count`.

## Reflection boundary

- Lookup order for reads: engine property list first, then C# property, then C#
  field, by exact name. Only public members resolve.
- Indexers are rejected, as are write-only members on read and read-only
  members on write.
- Method overloads match by argument count, with at most 16 arguments. Generic
  method definitions are skipped; params-array overloads pack trailing
  arguments into the array.
- Values serialize with full Variant fidelity in the editor; across the game
  channel arguments must be JSON-native values. Returned C# structs arrive as
  dictionaries carrying their type plus fields; deeper nesting collapses to a
  type marker.
- Editor calls need the `code_execute` gate; game calls need `code_execute`
  plus `game_runtime`.

## Export-build caution

The bridge is runtime reflection over exact member names. Members reached only
through the bridge must survive managed trimming in exported builds: keep them
referenced from compiled code or the lookup fails with a not-found error at
runtime. The fixed limits above (no generic definitions, no indexers, void
handlers with at most 4 parameters; params arrays are packed from trailing
arguments) apply on every build.

## Subscription tokens

`csharp_subscribe_event` returns a result token identifying the wiring on the
bridge instance that created it. Each call adds one more handler: subscribing
twice forwards each event twice. `csharp_unsubscribe_event` releases a wiring
early by token, so a wiring lasts until the token is released, the source
object is freed, or its bridge instance goes away; the bridge holds a reference
to the receiver while subscribed. Tokens are scoped to one bridge instance: the
editor bridge lives with the editor session, while the game bridge lives on the
`GdaCSharpRuntime` node, so restarting the game or reloading its scene drops
every subscription and you must subscribe again.

## Verifying a bridge call

A successful call is the response payload itself: the value, the method result
or the subscription token. A failed compile or a script error is not
represented by `new_errors_since_last_call`; use `build_csharp_assembly`'s
`success`/`errors` for compiler output, `get_debugger_log` for editor-side
script errors, and `get_debugger_errors(since_seq)` for running-game errors.

## See also

- SKILL.md - the C# development loop these tools extend
- tool-reference.md - one-line tool table for the loop
- godot-autopilot-runtime - channel, ready and capture semantics
- godot-autopilot-tools - discovery, call protocol and the full tool catalog
