# C# Reflection Bridge

The five reflection tools and the two bridge endpoints behind them. `csharp_get_property` reads a C# property or field, `csharp_call_method` calls a C# method, `csharp_subscribe_event` wires a receiver method to a C# event. Domain tools run through `call_tool`; confirm schemas with `get_tool_detail`. Reach for this reference when GDScript-side reads (`property_get`, `get_script_property`, `execute_game_script`) come back null or missing for a member you know exists in C#.

## Endpoints

- Editor (`runtime` editor, the default): the locator is resolved in the editor process and the call goes to the `GdaCSharpBridge` engine singleton. Needs a .NET editor build with the bridge registered (see Singleton registration below); without it the tools fail with a not-registered error telling you to register and retry.
- Game (`runtime` game): only scene-path locators are accepted, and the call is forwarded over the runtime channel as generated GDScript to the game bridge. Needs a running .NET game whose scene contains `GdaCSharpRuntime`; `object_id`, `object_id_str` and `memory` locators are rejected because editor-process handles cannot cross into the game.

Game-side plumbing: add a `GdaCSharpRuntime` node anywhere in the game scene. Its ready step creates a child node named `__gda_runtime` and stores a fresh `GdaCSharpBridge` instance in that child's metadata, so tool calls locate the holder from the tree root and read the bridge back from metadata. Failures are precise: no `__gda_runtime` node means the runtime node is missing from the running scene (add it and run a .NET game build); a holder without bridge metadata means its ready step did not run.

## Reflection boundary

- Lookup order for reads: engine property list first, then C# property, then C# field, by exact name. Only public members resolve unless the bridge instance was configured to allow non-public access.
- Indexers are rejected, as are write-only members on read and read-only members on write.
- Method overloads match by argument count, with at most 16 arguments. Generic method definitions are skipped; params-array overloads pack trailing arguments into the array.
- Values serialize with full Variant fidelity in the editor; across the game channel arguments must be JSON-native values. Returned C# structs arrive as dictionaries carrying their type plus fields; deeper nesting collapses to a type marker.
- Editor calls need the `code_execute` gate; game calls need `code_execute` plus `game_runtime`.

## Export-build caution

The bridge is runtime reflection over exact member names. Members reached only through the bridge must survive managed trimming in exported builds: keep them referenced from compiled code or the lookup fails with a not-found error at runtime. The fixed limits above (no generic definitions, no indexers, void handlers with at most 4 parameters; params arrays are packed from trailing arguments) apply on every build.

## Subscription tokens

`csharp_subscribe_event` returns a result token identifying the wiring on the bridge instance that created it. Each call adds one more handler: subscribing twice forwards each event twice. `csharp_unsubscribe_event` releases a wiring early by token, so a wiring lasts until the token is released, the source object is freed, or its bridge instance goes away; the bridge holds a reference to the receiver while subscribed. Tokens are scoped to one bridge instance: the editor singleton lives with the editor session, while the game bridge lives on the `GdaCSharpRuntime` node, so restarting the game or reloading its scene drops every subscription and you must subscribe again.

## Singleton registration

Editor-side calls need the engine singleton `GdaCSharpBridge` in the editor process; the plugin cannot register it from C++. Steps:

1. Ship the bridge source: deploy copies csharp_bridge/GdaCSharpBridge.cs next to the addon; include that file in the .NET project and build it.
2. Register once per editor session from C# (for example when an editor plugin enters the tree; the registration lasts until the editor exits or the singleton is removed):

```csharp
Engine.RegisterSingleton("GdaCSharpBridge", new GdaCSharpBridge());
```

3. Game side: add a `GdaCSharpRuntime` node to the game scene and run a .NET game build; no engine-singleton step is needed there because each runtime node carries its own bridge.
4. Verify with `csharp_get_property` on a known member and watch `new_errors_since_last_call` for a quiet fix.

## See also

- SKILL.md - the C# development loop these tools extend
- tool-reference.md - one-line tool table for the loop
- godot-autopilot-runtime - channel, ready and capture semantics
- godot-autopilot-tools - discovery, call protocol and the full tool catalog
