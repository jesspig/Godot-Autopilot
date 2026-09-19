# 用户工具约定示例（GDScript）。
# ① 把本文件（或同类脚本）放到 AutopilotTools.rescan(directory) 扫描的目录下，
#    rescan 会递归加载其中的 .gd 文件（上限 256 个文件、深度上限 8，跳过隐藏文件）。
# ② 脚本实例须提供实例方法 register_autopilot_tools(api)：rescan 会以
#    AutopilotTools 单例为参数调用它，在方法内用 api.register_tool 完成注册。
# ③ 工具名全局唯一：同名工具（含内置 MCP 工具）已存在时 register_tool 会拒绝
#    注册（返回 -1），需改名后重试；重复 rescan 同一目录不会产生重复注册。
extends RefCounted


func register_autopilot_tools(api):
	var definition = {
		"name": "echo_tool",
		"description": "Echo back the input value.",
		"category": "User",
		"tags": ["user", "sample", "echo"],
		"side_effect": "",
		"params": [
			{"name": "value", "type": "string", "description": "Value to echo back", "required": true}
		]
	}
	return api.register_tool(definition, Callable(self, "invoke"))


func invoke(args: Dictionary) -> Dictionary:
	return {"echo": args.get("value", "")}
