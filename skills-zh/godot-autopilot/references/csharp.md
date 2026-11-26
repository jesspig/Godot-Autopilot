# CSharp

## 前置检查

```bash
gda setup init --with-csharp-runtime
gda plugin install --with-csharp-runtime
gda doctor run --ci
```

C# 项目需要随插件安装运行时 autoload。构建前先探测环境，以便尽早报告缺失的 SDK 或构建工具。

## 操作：发现运行时工具

```bash
gda tool search "dotnet"
gda tool describe <tool>
gda tool call <tool> --args '<json>'
```

发现 .NET 环境与构建桥对应的确切工具；名字随 CLI 变化。描述它以读取 action 枚举与参数 schema，然后调用。不要凭记忆假定反射桥存在。

```json
{"godot": "dotnet", "path": "res://Project.csproj"}
```

## 操作：构建环

```bash
gda debug errors --level error
gda logs plugin --level error
```

用发现的工具构建，然后读调试器错误与插件日志。构建成功且运行时报告类型可实例化后，C# 改动才算生效。

## 观察

```bash
gda debug errors --level error
gda doctor run --ci
```

一起读取构建诊断与环境检查。

## 验证

宣布改动生效前确认构建干净、没有新调试器错误，且改动的类型在运行时可用。

## 失败分支

- 构建工具缺失：运行诊断并安装或暴露 SDK；不要猜路径。
- 改动未生效：重新构建并复查运行时诊断。
- 插件侧错误：读插件日志并上报。
