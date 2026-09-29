# Dynamic User Tools

Project scripts can register extra domain tools at runtime through the `AutopilotTools` singleton. A registered user tool surfaces through the same discovery protocol (`search_tools`, `get_tool_detail`) and the same `call_tool` path as every built-in domain tool, marked with `dynamic`. Both registration and invocation sit behind the `user_tools` capability.

## Registering and removing tools

Reach the singleton from GDScript with `Engine.get_singleton("AutopilotTools")`:

- `register_tool(definition, callable)` registers one tool and returns an integer handle; failure returns -1 and logs an error. The `definition` dictionary carries `name` (required, non-empty), `description`, `category` (defaults to `User`), `tags`, `side_effect` and `params` (a `name`/`type`/`description`/`required` list). A name that is already registered, or that collides with a built-in tool, is rejected.
- `unregister_tool(handle)` removes the tool registered under that handle.
- `has_tool(name)`, `list_tools()` and `get_tool(name)` query the registry; `get_tool` reports `name`, `description`, `category`, `tags`, `side_effect`, `params` and `handle`.
- `call_tool(name, args)` invokes any tool through the dispatch layer, including freshly registered user tools, without an MCP round trip.
- `is_enabled()` reports whether the `user_tools` capability is on; `set_enabled(enabled)` flips it in the plugin config.

The callable takes a single Dictionary of arguments and returns a Dictionary holding either the payload or an `error` object. A null return is an error, and a call whose target object was freed reports that the tool must be re-registered. Registering and unregistering rebuild the registry, so a new tool is immediately discoverable.

## The user_tools authorization gate

Registration and every user-tool call require the `user_tools` capability. Enable it by setting the `GODOT_AUTOPILOT_ALLOW` environment variable to `user_tools` (or `all`) and restarting the engine, or by ticking "Allow user tools" in the plugin MCP Config dock, which takes effect on the next call. When the environment variable is set it wins over the config. While the gate is off, registration is refused and calls answer that user tools are disabled.

## Directory rescan

`rescan(directory)` bulk-registers every user tool found under one directory (default res://addons/godot-autopilot-tools). The scan accepts only `res://` and `user://` roots, collects up to 256 `.gd` files no deeper than 8 levels while skipping hidden entries, then loads, instantiates and asks each file in isolation: the instance must expose `register_autopilot_tools(api)`, which calls `api.register_tool` for each tool it provides. One file's failure is recorded without aborting the scan, and the call returns `scanned`, `registered`, `failed` and `errors` (one `file`/`error` pair per failure). Re-scanning a directory registers nothing new, since duplicate names are rejected. Host long-lived tools on a RefCounted: after the scan, parentless Node instances and objects that are neither Node nor RefCounted are released.

## Discovery

`search_tools` finds user tools by name, description, category or tags like any other tool, and `get_tool_detail` marks them with `dynamic`. Prefer discovery over assuming a project tool exists: list first, read the schema, then call.

## C# notes

C# cannot subclass the GDExtension singleton type, so C# callers use duck typing: fetch the `AutopilotTools` singleton as a plain object and invoke `register_tool`, `call_tool` and `unregister_tool` by string method name with the same argument order. The project must be open in the .NET editor build with the C# assembly compiled, and the callable host must stay referenced. The scanner only reads `.gd` files, so C# tools are registered by explicit calls, never by rescan. Treat the C# snippets in the repository as documentation-level examples: they are not covered by automated verification.

## See also

- godot-autopilot-tools - discovery protocol, call path and authorization gates
- godot-autopilot-scripting - the script execution channels used to register tools from running code
