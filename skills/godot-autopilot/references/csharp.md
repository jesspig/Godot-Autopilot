# CSharp

## 前置检查

```bash
gda setup init --with-csharp-runtime
gda plugin install --with-csharp-runtime
gda doctor run --ci
```

C# projects need the runtime autoload installed with the plugin. Probe the
environment before a build so a missing SDK or build tool is reported early.

## 操作：发现运行时工具

```bash
gda tool search "dotnet"
gda tool describe <tool>
gda tool call <tool> --args '<json>'
```

Discover the exact tool for the .NET environment and build bridge; names change
with the CLI. Describe it for its action enum and argument schema, then call it.
Do not assume a reflection bridge exists from memory.

```json
{"godot": "dotnet", "path": "res://Project.csproj"}
```

## 操作：构建环

```bash
gda debug errors --level error
gda logs plugin --level error
```

Build through the discovered tool, then read debugger errors and plugin logs.
A C# change is not applied until the build succeeds and the runtime reports the
type as instantiable.

## 观察

```bash
gda debug errors --level error
gda doctor run --ci
```

Read the build diagnostics and the environment checks together.

## 验证

Confirm a clean build, no new debugger errors, and the changed type available at
runtime before declaring the change applied.

## 失败分支

- Build tool missing: run diagnostics and install or expose the SDK; do not
  guess a path.
- Change not applied: rebuild and re-check the runtime diagnostics.
- Plugin-side error: read the plugin log and report it.
