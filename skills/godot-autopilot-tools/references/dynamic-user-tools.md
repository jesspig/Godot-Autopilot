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

## Registering C# tools through the explicit wrapper

Two options were considered for the rescan gap (rescan only collects .gd files, so C# files never enter the scan): an explicit opt-in switch on rescan, or a thin C# wrapper beside register_tool. The wrapper was chosen because rescan, the file collector and the per-file register_autopilot_tools convention are the exact paths covered by the GDScript end-to-end cases 26_user_tools_code_mode and 27_user_tools_rescan; leaving them untouched keeps both green, while a rescan switch would rewire the shared scan loop and the file-type gate. So rescan stays .gd-only by design, and C# tools are registered by explicit calls, never by rescan.

C# callers use the register_csharp_tool wrapper instead of calling register_tool directly. It takes the same definition and callable arguments and returns a RegistrationHandle dictionary with three plain fields: tool_name (the registered name), handle (the integer handle, -1 on failure) and call_result (the get_tool snapshot proving the tool is immediately discoverable). The wrapper reuses the register_tool core, so validation, the user_tools gate, RefCounted host pinning, registry rebuild and duplicate handling are identical; only the Build gate and the return shape are new.

Build first: the assembly must already contain the C# callable. Build with `build_csharp_assembly` (wait=true) and confirm through `get_debugger_log` before registering. When the editor is not a .NET build (OS.has_feature mono is false) the wrapper returns a precise error telling you to open the project in the .NET editor build; when the C# runtime looks unloaded (CSharpScript class missing) it returns a precise error telling you to run the build and check the debugger log. Both are the same heuristics the write_file diagnostics and dotnet_environment use, and both are reported before anything is registered.

Duck typing is unchanged: fetch the AutopilotTools singleton as a plain object with Engine.GetSingleton and invoke the wrapper by string method name with the same argument order, building the callable with Callable.From on a long-lived host. Keep the host referenced on a RefCounted for the whole tool lifetime; after the host is freed, calls report that the tool must be re-registered. Registration and every call require the `user_tools` capability, exactly like the GDScript path.

Duplicate names are rejected idempotently: registering a name that is already registered, or that collides with a built-in tool, returns an error handle of -1 and registers nothing, so calling the wrapper twice with the same name never creates a second tool. Unregister with `unregister_tool` by handle, verify end to end with an explicit `call_tool` round trip, and prefer discovery (`search_tools`, `get_tool_detail`) over assuming the tool exists.

```csharp
using Godot;
using Godot.Collections;

public partial class EchoProbe : RefCounted
{
    public Dictionary Invoke(Dictionary args)
    {
        string value = args.TryGetValue("value", out Variant v) ? v.AsString() : "";
        return new Dictionary { { "echo", value } };
    }
}

public partial class CSharpToolBootstrap : Node
{
    private EchoProbe _probe;

    public override void _Ready()
    {
        GodotObject api = Engine.GetSingleton("AutopilotTools");
        if (api == null)
        {
            GD.PushError("AutopilotTools singleton not registered");
            return;
        }
        _probe = new EchoProbe();
        var definition = new Dictionary
        {
            { "name", "user_csharp_echo" },
            { "description", "Echo probe registered from C# via the explicit wrapper" },
            { "category", "User" },
            { "tags", new Array { "user", "csharp" } },
            { "side_effect", "" },
            { "params", new Array
                {
                    new Dictionary
                    {
                        { "name", "value" },
                        { "type", "string" },
                        { "description", "Value to echo back" },
                        { "required", true },
                    },
                }
            },
        };
        Dictionary reg = api.Call("register_csharp_tool", definition,
            Callable.From<Dictionary, Dictionary>(_probe.Invoke)).AsDictionary();
        string toolName = reg.TryGetValue("tool_name", out Variant n) ? n.AsString() : "";
        long handle = reg.TryGetValue("handle", out Variant h) ? h.AsInt64() : -1;
        if (handle <= 0)
        {
            GD.PushError($"register failed: {reg.TryGetValue("error", out Variant e)}");
            return;
        }
        Variant callResult = api.Call("call_tool", toolName,
            new Dictionary { { "value", "hi" } });
        var registrationHandle = new Dictionary
        {
            { "tool_name", toolName },
            { "handle", handle },
            { "call_result", callResult },
        };
        GD.Print($"registered: {registrationHandle}");
    }
}
```

## The user_tools authorization gate

Registration and every user-tool call require the `user_tools` capability. Enable it by setting the `GODOT_AUTOPILOT_ALLOW` environment variable to `user_tools` (or `all`) and restarting the engine, or by ticking "Allow user tools" in the plugin MCP Config dock, which takes effect on the next call. When the environment variable is set it wins over the config. While the gate is off, registration is refused and calls answer that user tools are disabled.

## Directory rescan

`rescan(directory)` bulk-registers every user tool found under one directory (default res://addons/godot-autopilot-tools). The scan accepts only `res://` and `user://` roots, collects up to 256 `.gd` files no deeper than 8 levels while skipping hidden entries, then loads, instantiates and asks each file in isolation: the instance must expose `register_autopilot_tools(api)`, which calls `api.register_tool` for each tool it provides. One file's failure is recorded without aborting the scan, and the call returns `scanned`, `registered`, `failed` and `errors` (one `file`/`error` pair per failure). Re-scanning a directory registers nothing new, since duplicate names are rejected. Host long-lived tools on a RefCounted: after the scan, parentless Node instances and objects that are neither Node nor RefCounted are released.

## Discovery

`search_tools` finds user tools by name, description, category or tags like any other tool, and `get_tool_detail` marks them with `dynamic`. Prefer discovery over assuming a project tool exists: list first, read the schema, then call.

## C# notes

C# cannot subclass the GDExtension singleton type, so C# callers use duck typing: fetch the `AutopilotTools` singleton as a plain object and invoke `call_tool` and `unregister_tool` by string method name with the same argument order, and register through the register_csharp_tool wrapper described above (same duck-typed call shape, plus the Build gate and the RegistrationHandle return). The project must be open in the .NET editor build with the C# assembly compiled, and the callable host must stay referenced. The scanner only reads `.gd` files, so C# tools are registered by explicit calls, never by rescan. Treat the C# snippets in the repository as documentation-level examples: they are not covered by automated verification.

## See also

- godot-autopilot-tools - discovery protocol, call path and authorization gates
- godot-autopilot-scripting - the script execution channels used to register tools from running code
